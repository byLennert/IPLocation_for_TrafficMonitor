#include "IPLocationPlugin.h"
#include "NetworkHelper.h"
#include "HtmlParser.h"
#include <chrono>
#include <utility>
#include <regex>
#include <iphlpapi.h>

// Singleton instance
CIPLocationPlugin& CIPLocationPlugin::Instance()
{
    static CIPLocationPlugin instance;
    return instance;
}

CIPLocationPlugin::CIPLocationPlugin()
    : m_stopThread(false), m_lastUpdateTime(0), m_isUpdating(false), m_newDataReady(false)
{
    m_name = L"IP Location";
    m_description = L"Display public IP and country from iplark.com";
    m_author = L"IPLocationPlugin";
    m_copyright = L"";
    m_version = L"1.0";
    m_url = L"https://iplark.com";

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
    // Check if new data is available
    if (m_newDataReady)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_nextIP.empty())
        {
            m_item.SetIPInfo(m_nextIP, m_nextCountry);
            m_tooltip = L"Public IP: " + m_nextIP;

            std::wstring location;
            if (!m_nextCity.empty() && m_nextCity != m_nextCountry)
                location = m_nextCity;
            else if (!m_nextRegion.empty() && m_nextRegion != m_nextCountry)
                location = m_nextRegion;
            else
                location = m_nextCountry;

            if (!location.empty())
                m_tooltip += L"\r\n地区: " + location;
        }
        else
        {
            m_item.SetStatus(m_nextStatus.empty() ? L"Failed" : m_nextStatus);
            m_tooltip = m_nextStatus.empty() ? L"Failed" : m_nextStatus;
        }
        m_newDataReady = false;
    }
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
        UpdateData();
        constexpr DWORD kRefreshIntervalMs = 30 * 1000;
        while (!m_stopThread)
        {
            DWORD waitResult = WAIT_TIMEOUT;
            if (m_wakeEvent)
                waitResult = WaitForSingleObject(m_wakeEvent, kRefreshIntervalMs);
            else
                Sleep(kRefreshIntervalMs);

            if (m_stopThread)
                return;

            if (waitResult == WAIT_OBJECT_0)
            {
                if (m_wakeEvent)
                    ResetEvent(m_wakeEvent);
            }

            UpdateData();
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

void CIPLocationPlugin::UpdateData()
{
    if (m_isUpdating.exchange(true))
        return; // Already updating
        
    // Fetch data
    // Update status if needed
    // But m_item is not thread safe, so we can't call SetStatus from here directly
    // unless we make SetStatus thread safe or use m_nextStatus logic.
    // For simplicity, we just fetch data.
    
    std::wstring ip;
    std::wstring country;
    std::wstring region;
    std::wstring city;
    std::wstring status;

    auto ipwho = NetworkHelper::HttpGet(L"https://ipwho.is/?lang=zh-CN", 8000);
    if (!ipwho.body.empty())
    {
        try
        {
            std::regex ipRe("\\\"ip\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
            std::regex countryRe("\\\"country\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
            std::regex regionRe("\\\"region\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
            std::regex cityRe("\\\"city\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
            std::smatch m;

            if (std::regex_search(ipwho.body, m, ipRe) && m.size() > 1)
                ip = NetworkHelper::Utf8ToWString(m[1].str());
            if (std::regex_search(ipwho.body, m, countryRe) && m.size() > 1)
                country = NetworkHelper::Utf8ToWString(m[1].str());
            if (std::regex_search(ipwho.body, m, regionRe) && m.size() > 1)
                region = NetworkHelper::Utf8ToWString(m[1].str());
            if (std::regex_search(ipwho.body, m, cityRe) && m.size() > 1)
                city = NetworkHelper::Utf8ToWString(m[1].str());
        }
        catch (...) {}
    }

    if (ip.empty())
    {
        auto ipify = NetworkHelper::HttpGet(L"https://api.ipify.org?format=json", 8000);
        if (!ipify.body.empty())
        {
            try
            {
                std::regex ipRe("\\\"ip\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
                std::smatch m;
                if (std::regex_search(ipify.body, m, ipRe) && m.size() > 1)
                    ip = NetworkHelper::Utf8ToWString(m[1].str());
            }
            catch (...) {}
        }
    }

    if (country.empty())
    {
        auto geo = NetworkHelper::HttpGet(L"http://ip-api.com/json/?fields=status,message,country,countryCode,query&lang=zh-CN", 8000);
        if (!geo.body.empty())
        {
            try
            {
                std::regex countryRe("\\\"country\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
                std::smatch m;
                if (std::regex_search(geo.body, m, countryRe) && m.size() > 1)
                    country = NetworkHelper::Utf8ToWString(m[1].str());
            }
            catch (...) {}
        }
    }

    if (ip.empty() && status.empty())
        status = L"Fetch failed";
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_nextIP = ip;
        m_nextCountry = country;
        m_nextRegion = region;
        m_nextCity = city;
        m_nextStatus = status;
        m_newDataReady = true;
    }
    
    m_isUpdating = false;
}

// Exported function
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &CIPLocationPlugin::Instance();
}
