#pragma once
#include <string>

struct HttpResponse
{
    std::string body;
    unsigned long status_code{};

    bool Ok() const { return status_code >= 200 && status_code < 300 && !body.empty(); }
};

class NetworkHelper
{
public:
    // Uses current-user proxy settings, including PAC/automatic detection.
    static HttpResponse HttpGet(const std::wstring& url, int timeout_ms = 5000);
    static std::wstring Utf8ToWString(const std::string& str);
};
