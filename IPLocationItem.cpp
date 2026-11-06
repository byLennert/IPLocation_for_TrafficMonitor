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
    // TrafficMonitor 1.86+ measures this text again each second using the
    // current taskbar font and DPI. Follow the full value as its country
    // changes, and leave margin for glyph overhang and rounding.
    thread_local std::wstring snapshot;
    std::lock_guard<std::mutex> lock(m_mutex);
    snapshot = m_displayText.empty() ? L"255.255.255.255 (Unknown)" : m_displayText;
    snapshot += L"  ";
    return snapshot.c_str();
}

void CIPLocationItem::SetIPInfo(const std::wstring& ip, const std::wstring& country)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_displayText = ip.empty() ? L"Failed" :
        ip + L" (" + (country.empty() ? L"Unknown" : country) + L")";
}

void CIPLocationItem::SetStatus(const std::wstring& status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_displayText = status.empty() ? L"Failed" : status;
}
