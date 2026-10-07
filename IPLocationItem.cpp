#include "IPLocationItem.h"
#include "IPLocationPlugin.h"

CIPLocationItem::CIPLocationItem()
{
}

CIPLocationItem::~CIPLocationItem()
{
}

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
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_displayText.empty())
        return L"Loading...";
    return m_displayText.c_str();
}

const wchar_t* CIPLocationItem::GetItemValueSampleText() const
{
    // Sizes the taskbar item. Only the address is shown, so the widest address is the
    // right sample; the old sample also reserved room for a country suffix.
    return L"255.255.255.255";
}

bool CIPLocationItem::IsCustomDraw() const
{
    return false; // Let TrafficMonitor handle drawing
}

int CIPLocationItem::GetItemWidth() const
{
    return 0;
}

void CIPLocationItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
}

int CIPLocationItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    return 0;
}

void CIPLocationItem::SetIPInfo(const std::wstring& ip, const std::wstring& country)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ip = ip;
    m_country = country;
    // The taskbar shows the address alone; the country/region belongs in the tooltip.
    m_displayText = m_ip.empty() ? kFailedText : m_ip;
}

void CIPLocationItem::SetStatus(const std::wstring& status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_status = status;
    // A failed refresh is reported as a failure. Keeping the previous address on screen
    // is what made a stale address look like a current one.
    m_ip.clear();
    m_country.clear();
    m_displayText = status.empty() ? kFailedText : status;
}
