#include "IPLocationItem.h"

#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <string>

namespace
{
int failures = 0;

void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

void CheckWidth(CIPLocationItem& item, HDC dc)
{
    const std::wstring value = item.GetItemValueText();
    const std::wstring sample = item.GetItemValueSampleText();
    for (const wchar_t* fontName : { L"Segoe UI", L"Microsoft YaHei UI", L"Georgia" })
    {
        for (int dpi : { 96, 120, 144, 192 })
        {
            HFONT font = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, fontName);
            Require(font != nullptr, "create taskbar test font");
            if (!font)
                continue;
            HGDIOBJ previous = SelectObject(dc, font);
            SIZE valueSize{};
            SIZE sampleSize{};
            const BOOL measuredValue = GetTextExtentPoint32W(dc, value.c_str(),
                static_cast<int>(value.size()), &valueSize);
            const BOOL measuredSample = GetTextExtentPoint32W(dc, sample.c_str(),
                static_cast<int>(sample.size()), &sampleSize);
            Require(measuredValue && measuredSample, "measure display and reserved text");
            Require(sampleSize.cx > valueSize.cx, "reserve the full display width plus padding");
            SelectObject(dc, previous);
            DeleteObject(font);
        }
    }
}
}

int main()
{
    HDC dc = CreateCompatibleDC(nullptr);
    Require(dc != nullptr, "create measurement DC");
    if (!dc)
        return 1;

    CIPLocationItem item;
    CheckWidth(item, dc);

    // Regression for issue #1: the old fixed sample was narrower than this
    // IP and country. Also cover localized and unusually long country names.
    item.SetIPInfo(L"110.185.36.126", L"CN");
    Require(std::wstring(item.GetItemValueText()) == L"110.185.36.126 (CN)",
        "keep the last IPv4 digit and country");
    CheckWidth(item, dc);
    for (const wchar_t* country : { L"中国", L"United States", L"Saint Vincent and the Grenadines" })
    {
        item.SetIPInfo(L"255.255.255.255", country);
        Require(std::wstring(item.GetItemValueText()) ==
            L"255.255.255.255 (" + std::wstring(country) + L")", "keep the complete country name");
        CheckWidth(item, dc);
    }

    item.SetIPInfo(L"1.1.1.1", L"");
    Require(std::wstring(item.GetItemValueText()) == L"1.1.1.1 (Unknown)",
        "show the IP while country lookup is pending");
    CheckWidth(item, dc);
    item.SetIPInfo(L"1.1.1.1", L"Australia");
    Require(std::wstring(item.GetItemValueText()) == L"1.1.1.1 (Australia)",
        "update the country after lookup completes");
    CheckWidth(item, dc);

    item.SetStatus(L"Failed");
    Require(std::wstring(item.GetItemValueText()) == L"Failed", "clear stale IP and country on failure");
    CheckWidth(item, dc);
    item.SetIPInfo(L"", L"CN");
    Require(std::wstring(item.GetItemValueText()) == L"Failed", "reject a country without an IP");

    DeleteDC(dc);
    if (failures == 0)
        std::puts("IP + country display widths passed at 100%, 125%, 150%, and 200% DPI.");
    return failures == 0 ? 0 : 1;
}
