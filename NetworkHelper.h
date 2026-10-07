#pragma once
#include <string>
#include <vector>
#include <functional>

struct HttpResponse
{
    std::string body;
    unsigned long status_code{};

    // Checks HTTP status and a nonempty body; does not validate response completeness.
    bool Ok() const { return status_code >= 200 && status_code < 300 && !body.empty(); }
};

class NetworkHelper
{
public:
    // Opens a new WinHTTP session and reads proxy settings for each GET.
    // timeout_ms applies to individual network stages, not the entire request.
    static HttpResponse HttpGet(const std::wstring& url, int timeout_ms = 5000);
    static std::wstring Utf8ToWString(const std::string& str);

    // Varies the query string to reduce reuse of cached responses between polls.
    static std::wstring WithCacheBuster(const std::wstring& url);
};
