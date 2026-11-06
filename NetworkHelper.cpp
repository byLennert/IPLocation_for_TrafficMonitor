#include "NetworkHelper.h"

#include <windows.h>
#include <winhttp.h>
#include <cwchar>
#include <vector>

namespace
{
constexpr size_t kMaxResponseBytes = 64 * 1024;

struct InternetHandle
{
    HINTERNET value{};

    explicit InternetHandle(HINTERNET handle = nullptr) : value(handle) {}
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }

    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;

    explicit operator bool() const { return value != nullptr; }
};

struct ProxyConfig
{
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG value{};

    ~ProxyConfig()
    {
        if (value.lpszAutoConfigUrl) GlobalFree(value.lpszAutoConfigUrl);
        if (value.lpszProxy) GlobalFree(value.lpszProxy);
        if (value.lpszProxyBypass) GlobalFree(value.lpszProxyBypass);
    }
};

void ApplyCurrentUserProxy(HINTERNET session, const std::wstring& url)
{
    ProxyConfig config;
    if (!WinHttpGetIEProxyConfigForCurrentUser(&config.value))
        return;

    if (config.value.lpszAutoConfigUrl || config.value.fAutoDetect)
    {
        WINHTTP_AUTOPROXY_OPTIONS options{};
        if (config.value.lpszAutoConfigUrl)
        {
            options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
            options.lpszAutoConfigUrl = config.value.lpszAutoConfigUrl;
        }
        else
        {
            options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
            options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
        }
        options.fAutoLogonIfChallenged = TRUE;

        WINHTTP_PROXY_INFO proxy{};
        if (WinHttpGetProxyForUrl(session, url.c_str(), &options, &proxy))
        {
            const BOOL applied = WinHttpSetOption(session, WINHTTP_OPTION_PROXY, &proxy, sizeof(proxy));
            if (proxy.lpszProxy) GlobalFree(proxy.lpszProxy);
            if (proxy.lpszProxyBypass) GlobalFree(proxy.lpszProxyBypass);
            if (applied)
                return;
        }
    }

    if (config.value.lpszProxy && *config.value.lpszProxy)
    {
        WINHTTP_PROXY_INFO proxy{};
        proxy.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        proxy.lpszProxy = config.value.lpszProxy;
        proxy.lpszProxyBypass = config.value.lpszProxyBypass;
        WinHttpSetOption(session, WINHTTP_OPTION_PROXY, &proxy, sizeof(proxy));
    }
}
}

HttpResponse NetworkHelper::HttpGet(const std::wstring& url, int timeout_ms)
{
    HttpResponse response;
    if (url.empty() || timeout_ms <= 0)
        return response;

    // WinHttpCrackUrl puts the query string in lpszExtraInfo, not lpszUrlPath.
    // Give both components buffers so ?format=json and ?lang=zh-CN reach the server.
    std::vector<wchar_t> scheme(url.size() + 1);
    std::vector<wchar_t> host(url.size() + 1);
    std::vector<wchar_t> path(url.size() + 1);
    std::vector<wchar_t> extra(url.size() + 1);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.lpszScheme = scheme.data();
    components.dwSchemeLength = static_cast<DWORD>(scheme.size());
    components.lpszHostName = host.data();
    components.dwHostNameLength = static_cast<DWORD>(host.size());
    components.lpszUrlPath = path.data();
    components.dwUrlPathLength = static_cast<DWORD>(path.size());
    components.lpszExtraInfo = extra.data();
    components.dwExtraInfoLength = static_cast<DWORD>(extra.size());

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components) ||
        components.dwHostNameLength == 0 ||
        (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS))
        return response;

    std::wstring objectPath = components.dwUrlPathLength == 0 ? L"/" :
        std::wstring(path.data(), components.dwUrlPathLength);
    objectPath.append(extra.data(), components.dwExtraInfoLength);

    InternetHandle session(WinHttpOpen(L"IPLocationPlugin/1.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
        return response;

    WinHttpSetTimeouts(session.value, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
    ApplyCurrentUserProxy(session.value, url);

    InternetHandle connection(WinHttpConnect(session.value, host.data(), components.nPort, 0));
    if (!connection)
        return response;

    DWORD flags = WINHTTP_FLAG_REFRESH;
    if (components.nScheme == INTERNET_SCHEME_HTTPS)
        flags |= WINHTTP_FLAG_SECURE;
    InternetHandle request(WinHttpOpenRequest(connection.value, L"GET", objectPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request)
        return response;

    const wchar_t* headers = L"Accept: text/plain, application/json;q=0.9\r\n"
                             L"Cache-Control: no-cache, no-store, max-age=0\r\n"
                             L"Pragma: no-cache\r\n";
    if (!WinHttpSendRequest(request.value, headers, static_cast<DWORD>(std::wcslen(headers)),
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.value, nullptr))
        return response;

    DWORD statusSize = sizeof(response.status_code);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &response.status_code, &statusSize, WINHTTP_NO_HEADER_INDEX))
        return response;

    if (response.status_code < 200 || response.status_code >= 300)
        return response;

    while (response.body.size() < kMaxResponseBytes)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available) || available == 0)
            break;

        const size_t remaining = kMaxResponseBytes - response.body.size();
        const DWORD toRead = static_cast<DWORD>((available < remaining) ? available : remaining);
        std::vector<char> buffer(toRead);
        DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer.data(), toRead, &read) || read == 0)
            break;
        response.body.append(buffer.data(), read);
    }

    return response;
}

std::wstring NetworkHelper::Utf8ToWString(const std::string& str)
{
    if (str.empty())
        return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (required <= 0)
        return {};
    std::wstring result(required, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            str.data(), static_cast<int>(str.size()), &result[0], required))
        return {};
    return result;
}
