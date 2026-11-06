#include "IPLocationPlugin.h"
#include "NetworkHelper.h"

#include <windows.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <future>
#include <regex>

namespace
{
constexpr auto kRefreshInterval = std::chrono::seconds(30);
constexpr auto kRetryInterval = std::chrono::seconds(10);
constexpr int kProbeTimeoutMs = 3000;
constexpr int kGeoTimeoutMs = 5000;

struct IPSource
{
    const wchar_t* url;
    const wchar_t* name;
};

constexpr std::array<IPSource, 5> kIPSources{{
    { L"https://ipv4.icanhazip.com/", L"icanhazip.com" },
    { L"https://api4.ipify.org/", L"ipify.org" },
    { L"https://v4.ident.me/", L"ident.me" },
    { L"https://ipinfo.io/ip", L"ipinfo.io" },
    { L"https://api.ip.sb/ip", L"ip.sb" },
}};

std::wstring Trim(const std::wstring& value)
{
    const wchar_t* whitespace = L" \t\n\r\f\v";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::wstring::npos)
        return {};
    const size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

bool IsIPv4(const std::wstring& ip)
{
    int parts = 0;
    int digits = 0;
    int value = 0;
    for (size_t i = 0; i <= ip.size(); ++i)
    {
        const wchar_t c = i == ip.size() ? L'.' : ip[i];
        if (c >= L'0' && c <= L'9')
        {
            if (++digits > 3)
                return false;
            value = value * 10 + (c - L'0');
            if (value > 255)
                return false;
        }
        else if (c == L'.')
        {
            if (digits == 0)
                return false;
            ++parts;
            digits = 0;
            value = 0;
        }
        else
        {
            return false;
        }
    }
    return parts == 4;
}

std::wstring JsonString(const std::string& json, const std::string& key)
{
    try
    {
        const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch match;
        if (std::regex_search(json, match, pattern) && match.size() > 1)
            return Trim(NetworkHelper::Utf8ToWString(match[1].str()));
    }
    catch (...) {}
    return {};
}

std::wstring CacheBustedUrl(const std::wstring& url)
{
    static std::atomic<unsigned long> counter{ 0 };
    const unsigned long long nonce = (GetTickCount64() << 16) ^ ++counter;
    return url + (url.find(L'?') == std::wstring::npos ? L"?" : L"&") +
        L"_=" + std::to_wstring(nonce);
}

HttpResponse Fetch(const std::wstring& url, int timeoutMs)
{
    return NetworkHelper::HttpGet(CacheBustedUrl(url), timeoutMs);
}

std::wstring CurrentTimeText()
{
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%02d:%02d:%02d", time.wHour, time.wMinute, time.wSecond);
    return buffer;
}
}

CIPLocationPlugin& CIPLocationPlugin::Instance()
{
    static CIPLocationPlugin instance;
    return instance;
}

CIPLocationPlugin::CIPLocationPlugin()
    : m_name(L"IP Location"),
      m_description(L"Display the current public IPv4 address and location"),
      m_author(L"byLennert"),
      m_version(L"1.1.0"),
      m_url(L"https://github.com/byLennert/IPLocation_for_TrafficMonitor")
{
    StartUpdateThread();
}

CIPLocationPlugin::~CIPLocationPlugin()
{
    m_stopThread = true;
    m_wakeCv.notify_all();
    if (m_updateThread.joinable())
        m_updateThread.join();
}

void* CIPLocationPlugin::GetPluginIcon()
{
    return nullptr;
}

IPluginItem* CIPLocationPlugin::GetItem(int index)
{
    return index == 0 ? &m_item : nullptr;
}

void CIPLocationPlugin::DataRequired()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_newDataReady)
        return;

    if (m_nextIP.empty())
    {
        const std::wstring status = m_nextStatus.empty() ? L"Failed" : m_nextStatus;
        m_item.SetStatus(status);
        m_tooltip = status;
        if (!m_lastGoodIP.empty())
            m_tooltip += L"\r\n上次成功: " + m_lastGoodTimeText + L" (" + m_lastGoodIP + L")";
        m_tooltip += L"\r\n检查时间: " + m_nextTimeText;
    }
    else
    {
        m_item.SetIPInfo(m_nextIP, m_nextCountry);
        m_lastGoodIP = m_nextIP;
        m_lastGoodTimeText = m_nextTimeText;
        m_tooltip = L"Public IP: " + m_nextIP;
        if (!m_nextCountry.empty())
            m_tooltip += L"\r\n国家: " + m_nextCountry;

        const std::wstring location = !m_nextCity.empty() ? m_nextCity :
            (!m_nextRegion.empty() ? m_nextRegion : m_nextCountry);
        if (!location.empty())
            m_tooltip += L"\r\n地区: " + location;
        m_tooltip += L"\r\n更新时间: " + m_nextTimeText;
        if (!m_nextSource.empty())
            m_tooltip += L"\r\n数据源: " + m_nextSource;
    }
    m_newDataReady = false;
}

const wchar_t* CIPLocationPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME: return m_name.c_str();
    case TMI_DESCRIPTION: return m_description.c_str();
    case TMI_AUTHOR: return m_author.c_str();
    case TMI_COPYRIGHT: return m_copyright.c_str();
    case TMI_VERSION: return m_version.c_str();
    case TMI_URL: return m_url.c_str();
    default: return L"";
    }
}

const wchar_t* CIPLocationPlugin::GetTooltipInfo()
{
    thread_local std::wstring snapshot;
    std::lock_guard<std::mutex> lock(m_mutex);
    snapshot = m_tooltip;
    return snapshot.c_str();
}

void CIPLocationPlugin::StartUpdateThread()
{
    m_updateThread = std::thread([this]() {
        bool succeeded = UpdateData();
        while (!m_stopThread)
        {
            std::unique_lock<std::mutex> lock(m_wakeMutex);
            m_wakeCv.wait_for(lock, succeeded ? kRefreshInterval : kRetryInterval,
                [this]() { return m_stopThread || m_forceUpdate; });
            if (m_stopThread)
                return;
            m_forceUpdate = false;
            lock.unlock();
            succeeded = UpdateData();
        }
    });
}

void CIPLocationPlugin::ForceUpdate()
{
    {
        std::lock_guard<std::mutex> lock(m_wakeMutex);
        m_forceUpdate = true;
    }
    m_wakeCv.notify_one();
}

std::wstring CIPLocationPlugin::FetchPublicIP(std::wstring& sourceNote)
{
    constexpr size_t batchSize = 3;
    std::array<std::wstring, batchSize> answers;
    std::array<std::future<std::wstring>, batchSize> requests;
    const size_t rotation = m_sourceRotation;

    for (size_t i = 0; i < batchSize; ++i)
    {
        const IPSource source = kIPSources[(rotation + i) % kIPSources.size()];
        requests[i] = std::async(std::launch::async, [source]() -> std::wstring {
            try
            {
                const HttpResponse response = Fetch(source.url, kProbeTimeoutMs);
                if (!response.Ok())
                    return {};
                const std::wstring candidate = Trim(NetworkHelper::Utf8ToWString(response.body));
                if (IsIPv4(candidate))
                    return candidate;
            }
            catch (...) {}
            return {};
        });
    }
    for (size_t i = 0; i < batchSize; ++i)
        answers[i] = requests[i].get();
    m_sourceRotation = (rotation + 1) % kIPSources.size();

    std::wstring best;
    size_t bestVotes = 0;
    size_t bestIndex = 0;
    size_t validAnswers = 0;
    for (const std::wstring& answer : answers)
        if (!answer.empty())
            ++validAnswers;

    for (size_t i = 0; i < batchSize; ++i)
    {
        if (answers[i].empty())
            continue;
        size_t votes = 0;
        for (const std::wstring& answer : answers)
            if (answer == answers[i])
                ++votes;
        if (votes > bestVotes)
        {
            best = answers[i];
            bestVotes = votes;
            bestIndex = i;
        }
    }

    if (!best.empty())
    {
        sourceNote = std::wstring(kIPSources[(rotation + bestIndex) % kIPSources.size()].name) +
            L" (" + std::to_wstring(bestVotes) + L"/" + std::to_wstring(validAnswers) + L" 一致)";
        return best;
    }

    // A JSON source is a last resort when the three plain-text services all fail.
    const HttpResponse fallback = Fetch(L"https://ipwho.is/", kProbeTimeoutMs);
    if (fallback.Ok())
    {
        const std::wstring candidate = JsonString(fallback.body, "ip");
        if (IsIPv4(candidate))
        {
            sourceNote = L"ipwho.is";
            return candidate;
        }
    }
    return {};
}

bool CIPLocationPlugin::FetchGeo(const std::wstring& ip)
{
    std::wstring country;
    std::wstring region;
    std::wstring city;

    const HttpResponse who = Fetch(L"https://ipwho.is/" + ip + L"?lang=zh-CN", kGeoTimeoutMs);
    if (who.Ok())
    {
        country = JsonString(who.body, "country");
        region = JsonString(who.body, "region");
        city = JsonString(who.body, "city");
    }

    // ip-api.com's free endpoint is HTTP only; use it solely as a geo fallback.
    if (country.empty() && !m_stopThread)
    {
        const HttpResponse geo = Fetch(L"http://ip-api.com/json/" + ip +
            L"?fields=status,country,regionName,city&lang=zh-CN", kGeoTimeoutMs);
        if (geo.Ok() && JsonString(geo.body, "status") == L"success")
        {
            country = JsonString(geo.body, "country");
            region = JsonString(geo.body, "regionName");
            city = JsonString(geo.body, "city");
        }
    }

    if (country.empty())
        return false;
    m_geoIP = ip;
    m_geoCountry = country;
    m_geoRegion = region;
    m_geoCity = city;
    return true;
}

void CIPLocationPlugin::Publish(const std::wstring& ip, const std::wstring& status,
    const std::wstring& sourceNote, const std::wstring& checkedAt)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool cachedGeo = !ip.empty() && ip == m_geoIP;
    m_nextIP = ip;
    m_nextCountry = cachedGeo ? m_geoCountry : L"";
    m_nextRegion = cachedGeo ? m_geoRegion : L"";
    m_nextCity = cachedGeo ? m_geoCity : L"";
    m_nextStatus = status;
    m_nextSource = sourceNote;
    m_nextTimeText = checkedAt;
    m_newDataReady = true;
}

bool CIPLocationPlugin::UpdateData()
{
    std::wstring ip;
    std::wstring sourceNote;
    try { ip = FetchPublicIP(sourceNote); }
    catch (...) { ip.clear(); }
    const std::wstring checkedAt = CurrentTimeText();

    if (m_stopThread)
        return false;
    if (ip.empty())
    {
        Publish(L"", L"Failed", L"", checkedAt);
        return false;
    }

    // Publish the new address before the slower location lookup completes.
    Publish(ip, L"", sourceNote, checkedAt);
    if (ip != m_geoIP && !m_stopThread)
    {
        bool found = false;
        try { found = FetchGeo(ip); }
        catch (...) {}
        if (found && !m_stopThread)
            Publish(ip, L"", sourceNote, checkedAt);
    }
    return true;
}

extern "C" ITMPlugin* TMPluginGetInstance()
{
    return &CIPLocationPlugin::Instance();
}
