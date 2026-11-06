#include "IPLocationItem.h"

const wchar_t* CIPLocationItem::GetItemName() const
{
    return L"IP Location";
}

const wchar_t* CIPLocationItem::GetItemId() const
{
    return L"IPLocationPlugin";
}

const wchar_t* CIPLocationItem::GetItemLableText() const
{
    return L"IP";
}

const wchar_t* CIPLocationItem::GetItemValueText() const
{
    thread_local std::wstring snapshot;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_displayText.empty())
        return L"Loading...";
    snapshot = m_displayText;
    return snapshot.c_str();
}

const wchar_t* CIPLocationItem::GetItemValueSampleText() const
{
    // TrafficMonitor uses this sample to reserve taskbar width. Leave margin
    // for proportional fonts and DPI rounding so the final digit is visible.
    return L"255.255.255.255  ";
}

void CIPLocationItem::SetIP(const std::wstring& ip)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_displayText = ip.empty() ? L"Failed" : ip;
}

void CIPLocationItem::SetStatus(const std::wstring& status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_displayText = status.empty() ? L"Failed" : status;
}
