#pragma once
#include "PluginInterface.h"
#include "IPLocationItem.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <windows.h>

class CIPLocationPlugin : public ITMPlugin
{
public:
    CIPLocationPlugin();
    virtual ~CIPLocationPlugin();

    static CIPLocationPlugin& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void* GetPluginIcon() override;
    void ForceUpdate();

private:
    void StartUpdateThread();
    bool UpdateData();   // returns true when a public address was obtained

    // Returns the most frequent valid address; a single valid response is sufficient.
    std::wstring FetchPublicIP(std::wstring& sourceNote);
    // Fills the worker-side geo cache for `ip`. Returns true when a country was found.
    bool FetchGeo(const std::wstring& ip);
    // Replaces the pending result for DataRequired(). A successful location lookup
    // publishes again with the same address and a new publication time.
    void Publish(const std::wstring& ip, const std::wstring& status, const std::wstring& sourceNote);

    void SignalWake();

private:
    CIPLocationItem m_item;
    std::thread m_updateThread;
    std::atomic<bool> m_stopThread;
    std::mutex m_mutex; // protects the pending result and tooltip updates

    // Written by the update thread, consumed by the host's monitoring thread in
    // DataRequired(). Both use m_mutex; the atomic flag only signals pending data.
    std::wstring m_nextIP;
    std::wstring m_nextCountry;
    std::wstring m_nextRegion;
    std::wstring m_nextCity;
    std::wstring m_nextStatus;
    std::wstring m_nextTimeText;
    std::wstring m_nextSource;
    std::atomic<bool> m_newDataReady;

    // Worker-thread state only.
    size_t m_sourceRotation{ 0 };   // which endpoints lead the next poll
    std::wstring m_lastPublishedIP; // preferred in ties, cleared after a failed poll
    std::wstring m_geoIP;           // single-entry cache: last successful location lookup
    std::wstring m_geoCountry;
    std::wstring m_geoRegion;
    std::wstring m_geoCity;

    // Updated only by DataRequired(), kept for the failure tooltip.
    std::wstring m_lastGoodIP;
    std::wstring m_lastGoodTimeText;

    std::wstring m_name;
    std::wstring m_description;
    std::wstring m_author;
    std::wstring m_copyright;
    std::wstring m_version;
    std::wstring m_url;

    std::wstring m_tooltip;

    HANDLE m_wakeEvent{};
};
