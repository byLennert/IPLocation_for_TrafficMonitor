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
    void UpdateData();
    void SignalWake();

private:
    CIPLocationItem m_item;
    std::thread m_updateThread;
    std::atomic<bool> m_stopThread;
    std::mutex m_mutex;
    unsigned long long m_lastUpdateTime;
    
    std::wstring m_nextIP;
    std::wstring m_nextCountry;
    std::wstring m_nextRegion;
    std::wstring m_nextCity;
    std::wstring m_nextStatus;
    std::atomic<bool> m_newDataReady;
    std::atomic<bool> m_isUpdating;

    std::wstring m_name;
    std::wstring m_description;
    std::wstring m_author;
    std::wstring m_copyright;
    std::wstring m_version;
    std::wstring m_url;

    std::wstring m_tooltip;

    HANDLE m_wakeEvent{};
};
