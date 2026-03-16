#include "HtmlParser.h"
#include <regex>

std::wstring HtmlParser::ParseIP(const std::wstring& html)
{
    // Regex for IP: <span class="ip-highlight" ...>139.59.229.126</span>
    // Note: The class name is "ip-highlight".
    // We use a regex to capture the content inside the span.
    try {
        // Match <span ... class="ip-highlight" ...>IP</span>
        // But simplified: find class="ip-highlight" inside span tag
        std::wregex ip_regex(L"<span[^>]*class=\"ip-highlight\"[^>]*>([^<]+)</span>");
        std::wsmatch match;
        if (std::regex_search(html, match, ip_regex))
        {
            if (match.size() > 1)
            {
                return match[1].str();
            }
        }
    } catch (...) {
        // Fallback or log error
    }
    return L"";
}

std::wstring HtmlParser::ParseCountry(const std::wstring& html)
{
    // Regex for Country: 
    // Target element: class="info-item" ... <label>国家/地区</label> ... <div class="value">...</div>
    // We need to find the "info-item" block containing "国家/地区" label, then find the value div.
    
    // Strategy:
    // 1. Find all "info-item" blocks (this is hard with regex due to nesting).
    // 2. Simplification: Find "国家/地区" first, then look backwards for "info-item" (risky) or look around.
    // Better: Search for "国家/地区" then find the next <div class="value">...</div>
    
    // Let's try to find "国家/地区" and then the next "value" div.
    // HTML structure:
    // <div class="info-item">
    //   <div class="icon">...</div>
    //   <label>国家/地区</label>
    //   <div class="value">
    //      <img ...>
    //      Singapore
    //   </div>
    // </div>
    
    try {
        size_t labelPos = html.find(L"国家/地区");
        if (labelPos != std::wstring::npos)
        {
            // Find the next <div class="value"> after the label
            size_t valueDivStart = html.find(L"<div class=\"value\"", labelPos);
            if (valueDivStart != std::wstring::npos)
            {
                size_t contentStart = html.find(L">", valueDivStart);
                if (contentStart != std::wstring::npos)
                {
                    contentStart++; // Skip '>'
                    size_t divEnd = html.find(L"</div>", contentStart);
                    if (divEnd != std::wstring::npos)
                    {
                        std::wstring rawContent = html.substr(contentStart, divEnd - contentStart);
                        
                        // Clean up: remove <img> tags or other tags
                        // We only want the text.
                        // Simple tag stripper: remove <...>
                        std::wstring cleanContent;
                        bool insideTag = false;
                        for (wchar_t c : rawContent)
                        {
                            if (c == L'<') insideTag = true;
                            else if (c == L'>') insideTag = false;
                            else if (!insideTag) cleanContent += c;
                        }
                        
                        // Trim whitespace
                        const wchar_t* ws = L" \t\n\r\f\v";
                        cleanContent.erase(cleanContent.find_last_not_of(ws) + 1);
                        cleanContent.erase(0, cleanContent.find_first_not_of(ws));
                        
                        return cleanContent;
                    }
                }
            }
        }
    } catch (...) {
        // Fallback
    }
    return L"";
}

std::wstring HtmlParser::ExtractTextBetween(const std::wstring& html, const std::wstring& startTag, const std::wstring& endTag)
{
    // Not used in current implementation but kept for reference
    return L"";
}
