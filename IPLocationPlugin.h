#pragma once

#include "PluginInterface.h"
#include "IPLocationItem.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class CIPLocationPlugin : public ITMPlugin
{
public:
    CIPLocationPlugin();
    ~CIPLocationPlugin();

    static CIPLocationPlugin& Instance();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    void* GetPluginIcon() override;
    void ForceUpdate();

private:
    void StartUpdateThread();
    bool UpdateData();
    std::wstring FetchPublicIP(std::wstring& sourceNote);
    bool FetchGeo(const std::wstring& ip);
    void Publish(const std::wstring& ip, const std::wstring& status,
        const std::wstring& sourceNote, const std::wstring& checkedAt);

    CIPLocationItem m_item;
    std::thread m_updateThread;
    std::atomic<bool> m_stopThread{ false };
    std::mutex m_wakeMutex;
    std::condition_variable m_wakeCv;
    bool m_forceUpdate{};

    // Pending data and tooltip are shared with TrafficMonitor's polling thread.
    std::mutex m_mutex;
    bool m_newDataReady{};
    std::wstring m_nextIP;
    std::wstring m_nextCountry;
    std::wstring m_nextRegion;
    std::wstring m_nextCity;
    std::wstring m_nextStatus;
    std::wstring m_nextSource;
    std::wstring m_nextTimeText;
    std::wstring m_tooltip;
    std::wstring m_lastGoodIP;
    std::wstring m_lastGoodTimeText;

    // Only the update thread accesses the source rotation and geo cache.
    size_t m_sourceRotation{};
    std::wstring m_geoIP;
    std::wstring m_geoCountry;
    std::wstring m_geoRegion;
    std::wstring m_geoCity;

    std::wstring m_name;
    std::wstring m_description;
    std::wstring m_author;
    std::wstring m_copyright;
    std::wstring m_version;
    std::wstring m_url;
};
