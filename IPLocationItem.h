#pragma once
#include "PluginInterface.h"
#include <string>
#include <mutex>

class CIPLocationItem : public IPluginItem
{
public:
    CIPLocationItem();
    virtual ~CIPLocationItem();

    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidth() const override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
    virtual int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

    void SetIPInfo(const std::wstring& ip, const std::wstring& country);
    void SetStatus(const std::wstring& status);

private:
    mutable std::mutex m_mutex;
    std::wstring m_ip;
    std::wstring m_country;
    std::wstring m_displayText;
    std::wstring m_status;
};
