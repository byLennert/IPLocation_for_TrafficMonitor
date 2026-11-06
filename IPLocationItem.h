#pragma once

#include "PluginInterface.h"

#include <mutex>
#include <string>

class CIPLocationItem : public IPluginItem
{
public:
    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;

    void SetIP(const std::wstring& ip);
    void SetStatus(const std::wstring& status);

private:
    mutable std::mutex m_mutex;
    std::wstring m_displayText;
};
