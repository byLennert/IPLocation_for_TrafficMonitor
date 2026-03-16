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
    return L"127.0.0.1 (CN)";
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
    if (m_ip.empty())
        m_displayText = L"Failed";
    else
        m_displayText = m_ip + L" (" + (m_country.empty() ? L"Unknown" : m_country) + L")";
}

void CIPLocationItem::SetStatus(const std::wstring& status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_status = status;
    // Optionally update display text with status if IP is empty
    if (m_ip.empty())
        m_displayText = status;
}
