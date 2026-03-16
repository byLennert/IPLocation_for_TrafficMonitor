#pragma once
#include <string>

class HtmlParser
{
public:
    static std::wstring ParseIP(const std::wstring& html);
    static std::wstring ParseCountry(const std::wstring& html);

private:
    static std::wstring ExtractTextBetween(const std::wstring& html, const std::wstring& startTag, const std::wstring& endTag);
};
