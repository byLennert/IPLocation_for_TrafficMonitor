#pragma once
#include <string>
#include <vector>
#include <functional>

struct HttpResponse
{
    std::string body;
    unsigned long status_code{};
};

class NetworkHelper
{
public:
    static HttpResponse HttpGet(const std::wstring& url, int timeout_ms = 5000);
    static std::wstring Utf8ToWString(const std::string& str);
};
