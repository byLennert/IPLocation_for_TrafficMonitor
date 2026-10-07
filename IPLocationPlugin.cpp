#include "IPLocationPlugin.h"
#include "NetworkHelper.h"
#include <regex>
#include <string>
#include <vector>
#include <iterator>

namespace
{
    // How long to wait between polls. A failed poll is retried sooner so a transient
    // network hiccup does not leave the item reading "Failed" for a full interval.
    constexpr DWORD kRefreshIntervalMs = 30 * 1000;
    constexpr DWORD kRetryIntervalMs = 10 * 1000;
    // Deliberately short: several endpoints are queried at once, so waiting a long time
    // on any single one buys nothing, and a dead endpoint costs more than this value
    // anyway (see the timeout note in NetworkHelper::HttpGet).
    constexpr int kProbeTimeoutMs = 3000;
    constexpr int kGeoTimeoutMs = 8000;

    // Number of endpoints queried simultaneously in each rotating batch.
    constexpr size_t kProbesPerPoll = 3;

    struct IPSource
    {
        const wchar_t* url;
        const wchar_t* name;
    };

    // Endpoints that answer with the bare address as plain text.
    const IPSource kPlainTextIPSources[] = {
        { L"https://ipv4.icanhazip.com/", L"icanhazip.com" },
        { L"https://api4.ipify.org/",     L"ipify.org" },
        { L"https://v4.ident.me/",        L"ident.me" },
        { L"https://ipinfo.io/ip",        L"ipinfo.io" },
        { L"https://api.ip.sb/ip",        L"ip.sb" },
    };

    struct JsonIPSource
    {
        const wchar_t* url;
        const wchar_t* name;
        const char* field;
    };

    // JSON endpoints used only when the selected plain-text batch has no valid IPv4.
    const JsonIPSource kJsonIPSources[] = {
        { L"http://ip-api.com/json/?fields=status,message,query", L"ip-api.com", "query" },
        { L"https://ipwho.is/",                                   L"ipwho.is",   "ip" },
    };

    std::wstring TrimW(const std::wstring& s)
    {
        const wchar_t* ws = L" \t\n\r\f\v";
        const auto start = s.find_first_not_of(ws);
        if (start == std::wstring::npos)
            return L"";
        const auto end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }

    bool IsIPv4(const std::wstring& ip)
    {
        int partCount = 0;
        int value = 0;
        int digits = 0;
        for (size_t i = 0; i <= ip.size(); ++i)
        {
            const wchar_t c = (i < ip.size()) ? ip[i] : L'.';
            if (c >= L'0' && c <= L'9')
            {
                value = value * 10 + (c - L'0');
                if (++digits > 3)
                    return false;
                if (value > 255)
                    return false;
            }
            else if (c == L'.')
            {
                if (digits == 0)
                    return false;
                ++partCount;
                value = 0;
                digits = 0;
            }
            else
            {
                return false;
            }
        }
        return partCount == 4;
    }

    // Pulls a string value out of a flat JSON response. The key is matched with its
    // quotes so that "country" cannot be satisfied by "country_code".
    std::wstring JsonString(const std::string& json, const std::string& key)
    {
        try
        {
            const std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
            std::smatch m;
            if (std::regex_search(json, m, re) && m.size() > 1)
                return TrimW(NetworkHelper::Utf8ToWString(m[1].str()));
        }
        catch (...) {}
        return std::wstring();
    }

    HttpResponse Fetch(const std::wstring& url, int timeout_ms)
    {
        // Vary the URL as well as sending no-cache headers to reduce stale cache hits.
        return NetworkHelper::HttpGet(NetworkHelper::WithCacheBuster(url), timeout_ms);
    }

    std::wstring CurrentTimeText()
    {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t buf[16] = {};
        swprintf_s(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
        return buf;
    }
}

// Singleton instance
CIPLocationPlugin& CIPLocationPlugin::Instance()
{
    static CIPLocationPlugin instance;
    return instance;
}

CIPLocationPlugin::CIPLocationPlugin()
    : m_stopThread(false), m_newDataReady(false)
{
    m_name = L"IP Location";
    m_description = L"Display the current public IPv4 address";
    m_author = L"IPLocationPlugin";
    m_copyright = L"";
    m_version = L"1.2";
    m_url = L"https://github.com/kai2837619550/IPLocation_for_TrafficMonitor";

    m_wakeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    StartUpdateThread();
}

CIPLocationPlugin::~CIPLocationPlugin()
{
    m_stopThread = true;
    SignalWake();
    if (m_updateThread.joinable())
        m_updateThread.join();

    if (m_wakeEvent)
    {
        CloseHandle(m_wakeEvent);
        m_wakeEvent = nullptr;
    }
}

void* CIPLocationPlugin::GetPluginIcon()
{
    return nullptr;
}

IPluginItem* CIPLocationPlugin::GetItem(int index)
{
    if (index == 0)
        return &m_item;
    return nullptr;
}

void CIPLocationPlugin::DataRequired()
{
    if (!m_newDataReady)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_nextIP.empty())
    {
        m_item.SetIPInfo(m_nextIP, m_nextCountry);

        m_lastGoodIP = m_nextIP;
        m_lastGoodTimeText = m_nextTimeText;

        m_tooltip = L"公网 IP: " + m_nextIP;

        std::wstring location;
        if (!m_nextCity.empty() && m_nextCity != m_nextCountry)
            location = m_nextCity;
        else if (!m_nextRegion.empty() && m_nextRegion != m_nextCountry)
            location = m_nextRegion;
        else
            location = m_nextCountry;

        if (!location.empty())
            m_tooltip += L"\r\n地区: " + location;

        m_tooltip += L"\r\nIP地址更新时间: " + m_nextTimeText;
        if (!m_nextSource.empty())
            m_tooltip += L"\r\n数据源: " + m_nextSource;
    }
    else
    {
        const std::wstring status = m_nextStatus.empty() ? CIPLocationItem::kFailedText : m_nextStatus;
        m_item.SetStatus(status);

        m_tooltip = status;
        // The address itself is no longer displayed once a refresh fails, but showing
        // when it was last confirmed makes the failure diagnosable.
        if (!m_lastGoodIP.empty())
            m_tooltip += L"\r\n上次成功: " + m_lastGoodTimeText + L" (" + m_lastGoodIP + L")";
        m_tooltip += L"\r\n检查时间: " + m_nextTimeText;
    }
    m_newDataReady = false;
}

const wchar_t* CIPLocationPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return m_name.c_str();
    case TMI_DESCRIPTION:
        return m_description.c_str();
    case TMI_AUTHOR:
        return m_author.c_str();
    case TMI_COPYRIGHT:
        return m_copyright.c_str();
    case TMI_VERSION:
        return m_version.c_str();
    case TMI_URL:
        return m_url.c_str();
    default:
        return L"";
    }
}

const wchar_t* CIPLocationPlugin::GetTooltipInfo()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tooltip.c_str();
}

void CIPLocationPlugin::StartUpdateThread()
{
    m_updateThread = std::thread([this]() {
        bool succeeded = UpdateData();
        while (!m_stopThread)
        {
            const DWORD interval = succeeded ? kRefreshIntervalMs : kRetryIntervalMs;

            DWORD waitResult = WAIT_TIMEOUT;
            if (m_wakeEvent)
                waitResult = WaitForSingleObject(m_wakeEvent, interval);
            else
                Sleep(interval);

            if (m_stopThread)
                return;

            if (waitResult == WAIT_OBJECT_0 && m_wakeEvent)
                ResetEvent(m_wakeEvent);

            succeeded = UpdateData();
        }
    });
}

void CIPLocationPlugin::ForceUpdate()
{
    SignalWake();
}

void CIPLocationPlugin::SignalWake()
{
    if (m_wakeEvent)
        SetEvent(m_wakeEvent);
}

std::wstring CIPLocationPlugin::FetchPublicIP(std::wstring& sourceNote)
{
    const size_t total = std::size(kPlainTextIPSources);
    const size_t batch = (total < kProbesPerPoll) ? total : kProbesPerPoll;

    // Advance the three-source window by one after each poll. Consecutive batches
    // overlap, but all sources participate over a full rotation.
    std::vector<size_t> picked(batch);
    for (size_t k = 0; k < batch; ++k)
        picked[k] = (m_sourceRotation + k) % total;

    // Parallel requests avoid adding their durations together. Joining all probes
    // still makes the batch wait for its slowest request.
    std::vector<std::wstring> answers(batch);
    std::vector<std::thread> threads;
    threads.reserve(batch);
    for (size_t k = 0; k < batch; ++k)
    {
        threads.emplace_back([k, &answers, &picked]() {
            try
            {
                const HttpResponse resp = Fetch(kPlainTextIPSources[picked[k]].url, kProbeTimeoutMs);
                if (!resp.Ok())
                    return;
                const std::wstring candidate = TrimW(NetworkHelper::Utf8ToWString(resp.body));
                if (IsIPv4(candidate))
                    answers[k] = candidate;
            }
            catch (...) {}
        });
    }
    for (auto& t : threads)
        t.join();

    m_sourceRotation = (m_sourceRotation + 1) % total;

    // Pick the most frequent valid address, without requiring a strict majority.
    // Ties favour the last published IP if present; otherwise the first tied answer wins.
    std::wstring best;
    size_t bestVotes = 0;
    size_t bestSource = 0;
    size_t valid = 0;
    for (const std::wstring& a : answers)
    {
        if (!a.empty())
            ++valid;
    }
    for (size_t k = 0; k < batch; ++k)
    {
        if (answers[k].empty())
            continue;

        size_t votes = 0;
        for (const std::wstring& a : answers)
        {
            if (a == answers[k])
                ++votes;
        }

        const bool keepsCurrent = (answers[k] == m_lastPublishedIP && best != m_lastPublishedIP);
        if (votes > bestVotes || (votes == bestVotes && keepsCurrent))
        {
            best = answers[k];
            bestVotes = votes;
            bestSource = picked[k];
        }
    }

    if (!best.empty())
    {
        sourceNote = std::wstring(kPlainTextIPSources[bestSource].name)
            + L" (" + std::to_wstring(bestVotes) + L"/" + std::to_wstring(valid) + L" 一致)";
        return best;
    }

    // No valid IPv4 in this batch; try the JSON sources in order.
    for (const JsonIPSource& source : kJsonIPSources)
    {
        if (m_stopThread)
            break;

        const HttpResponse resp = Fetch(source.url, kProbeTimeoutMs);
        if (!resp.Ok())
            continue;

        const std::wstring candidate = JsonString(resp.body, source.field);
        if (IsIPv4(candidate))
        {
            sourceNote = source.name;
            return candidate;
        }
    }

    return std::wstring();
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

    if (country.empty() && !m_stopThread)
    {
        const HttpResponse geo = Fetch(
            L"http://ip-api.com/json/" + ip + L"?fields=status,message,country,regionName,city,query&lang=zh-CN",
            kGeoTimeoutMs);
        if (geo.Ok())
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

void CIPLocationPlugin::Publish(const std::wstring& ip, const std::wstring& status, const std::wstring& sourceNote)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nextIP = ip;

    const bool geoMatches = !ip.empty() && ip == m_geoIP;
    m_nextCountry = geoMatches ? m_geoCountry : std::wstring();
    m_nextRegion = geoMatches ? m_geoRegion : std::wstring();
    m_nextCity = geoMatches ? m_geoCity : std::wstring();

    m_nextStatus = status;
    m_nextSource = sourceNote;
    m_nextTimeText = CurrentTimeText();
    m_newDataReady = true;
}

bool CIPLocationPlugin::UpdateData()
{
    // Called only by the update thread. Address lookup exceptions become a failed poll.
    std::wstring ip;
    std::wstring sourceNote;
    try
    {
        ip = FetchPublicIP(sourceNote);
    }
    catch (...)
    {
        ip.clear();
    }

    if (ip.empty())
    {
        m_lastPublishedIP.clear();
        Publish(std::wstring(), CIPLocationItem::kFailedText, std::wstring());
        return false;
    }

    // Make the address available to DataRequired() before querying its location.
    m_lastPublishedIP = ip;
    Publish(ip, std::wstring(), sourceNote);

    // Reuse the last successful location when its IP matches. Failures are retried
    // on the next poll. This synchronous lookup delays the start of the interval wait.
    if (ip != m_geoIP && !m_stopThread)
    {
        bool haveGeo = false;
        try
        {
            haveGeo = FetchGeo(ip);
        }
        catch (...)
        {
        }

        if (haveGeo)
            Publish(ip, std::wstring(), sourceNote);
    }

    return true;
}

// Exported function
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &CIPLocationPlugin::Instance();
}
