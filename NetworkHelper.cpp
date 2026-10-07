#include "NetworkHelper.h"
#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace
{
    // Bodies from these endpoints are a handful of bytes; anything larger is an error
    // page or a captive portal and there is no reason to keep reading it.
    constexpr size_t kMaxBodyBytes = 256 * 1024;

    struct UrlParts
    {
        std::wstring host;
        std::wstring path;
        INTERNET_PORT port{};
        bool secure{};
    };

    // WinHttpCrackUrl only returns the components it is given a buffer for. The previous
    // version passed no buffer for the extra info, so every query string was silently
    // dropped: "?format=json", "?fields=..." and "?lang=zh-CN" never reached the server.
    bool CrackUrl(const std::wstring& url, UrlParts& parts)
    {
        URL_COMPONENTS comp{};
        comp.dwStructSize = sizeof(comp);

        std::vector<wchar_t> host(url.size() + 1, 0);
        std::vector<wchar_t> path(url.size() + 1, 0);
        std::vector<wchar_t> extra(url.size() + 1, 0);

        comp.lpszHostName = host.data();
        comp.dwHostNameLength = static_cast<DWORD>(host.size());
        comp.lpszUrlPath = path.data();
        comp.dwUrlPathLength = static_cast<DWORD>(path.size());
        comp.lpszExtraInfo = extra.data();
        comp.dwExtraInfoLength = static_cast<DWORD>(extra.size());
        comp.dwSchemeLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.length()), 0, &comp))
            return false;

        parts.host.assign(comp.lpszHostName, comp.dwHostNameLength);
        parts.path.assign(comp.lpszUrlPath, comp.dwUrlPathLength);
        parts.path.append(comp.lpszExtraInfo, comp.dwExtraInfoLength);
        if (parts.path.empty())
            parts.path = L"/";
        parts.port = comp.nPort;
        parts.secure = (comp.nScheme == INTERNET_SCHEME_HTTPS);
        return !parts.host.empty();
    }

    // The old code only honoured a statically configured proxy. When the system is set to
    // "automatically detect settings" or uses a PAC script, the request bypassed the proxy
    // and reported the raw ISP address instead of the one the rest of the system uses.
    void ApplyProxyConfig(HINTERNET hSession, const std::wstring& url)
    {
        WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxy{};
        if (!WinHttpGetIEProxyConfigForCurrentUser(&ieProxy))
            return;

        bool applied = false;
        if (ieProxy.fAutoDetect || ieProxy.lpszAutoConfigUrl)
        {
            WINHTTP_AUTOPROXY_OPTIONS autoOptions{};
            if (ieProxy.lpszAutoConfigUrl)
            {
                autoOptions.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
                autoOptions.lpszAutoConfigUrl = ieProxy.lpszAutoConfigUrl;
            }
            else
            {
                autoOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
                autoOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
            }
            autoOptions.fAutoLogonIfChallenged = TRUE;

            WINHTTP_PROXY_INFO proxyInfo{};
            if (WinHttpGetProxyForUrl(hSession, url.c_str(), &autoOptions, &proxyInfo))
            {
                // WinHttpSetOption copies the strings, so they can be freed right after.
                applied = (WinHttpSetOption(hSession, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo)) != FALSE);
                if (proxyInfo.lpszProxy) GlobalFree(proxyInfo.lpszProxy);
                if (proxyInfo.lpszProxyBypass) GlobalFree(proxyInfo.lpszProxyBypass);
            }
        }

        if (!applied && ieProxy.lpszProxy && *ieProxy.lpszProxy)
        {
            WINHTTP_PROXY_INFO proxyInfo{};
            proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
            proxyInfo.lpszProxy = ieProxy.lpszProxy;
            proxyInfo.lpszProxyBypass = ieProxy.lpszProxyBypass;
            WinHttpSetOption(hSession, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo));
        }

        if (ieProxy.lpszAutoConfigUrl) GlobalFree(ieProxy.lpszAutoConfigUrl);
        if (ieProxy.lpszProxy) GlobalFree(ieProxy.lpszProxy);
        if (ieProxy.lpszProxyBypass) GlobalFree(ieProxy.lpszProxyBypass);
    }
}

HttpResponse NetworkHelper::HttpGet(const std::wstring& url, int timeout_ms)
{
    HttpResponse resp;

    UrlParts parts;
    if (!CrackUrl(url, parts))
        return resp;

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0 Safari/537.36",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return resp;

    // Note for callers: an unreachable host does not cost exactly timeout_ms. Measured on
    // Windows 11, 3000 gives up after ~4s but 5000 takes ~10s, because WinHTTP makes a
    // second attempt. (WINHTTP_OPTION_CONNECT_RETRIES reports success but changes nothing,
    // and lowering only the resolve/connect timeouts does not help either.) Keep the
    // timeout small wherever a dead endpoint is a realistic outcome.
    WinHttpSetTimeouts(hSession, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    secureProtocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    // TODO: WINHTTP_OPTION_DISABLE_FEATURE requires a request handle; this session
    // call currently fails, so it does not disable keep-alive or cookies.
    DWORD disableFeatures = WINHTTP_DISABLE_KEEP_ALIVE | WINHTTP_DISABLE_COOKIES;
    WinHttpSetOption(hSession, WINHTTP_OPTION_DISABLE_FEATURE, &disableFeatures, sizeof(disableFeatures));

    ApplyProxyConfig(hSession, url);

    HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
    HINTERNET hRequest = nullptr;

    if (hConnect)
    {
        DWORD flags = WINHTTP_FLAG_REFRESH; // asks proxies to revalidate against the origin
        if (parts.secure)
            flags |= WINHTTP_FLAG_SECURE;

        hRequest = WinHttpOpenRequest(hConnect, L"GET", parts.path.c_str(), NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    }

    if (hRequest)
    {
        const std::wstring headers =
            L"Accept: text/plain, application/json;q=0.9, */*;q=0.8\r\n"
            L"Accept-Language: zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
            L"Cache-Control: no-cache, no-store, max-age=0\r\n"
            L"Pragma: no-cache\r\n";

        BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.length()),
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

        if (bResults)
            bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (bResults)
        {
            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
            {
                resp.status_code = statusCode;
            }

            DWORD dwSize = 0;
            do
            {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                    break;
                if (dwSize == 0)
                    break;

                if (resp.body.size() + dwSize > kMaxBodyBytes)
                    dwSize = static_cast<DWORD>(kMaxBodyBytes - resp.body.size());
                if (dwSize == 0)
                    break;

                std::vector<char> buffer(dwSize);
                DWORD dwDownloaded = 0;
                if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded))
                    break;
                if (dwDownloaded == 0)
                    break;

                resp.body.append(buffer.data(), dwDownloaded);
            } while (resp.body.size() < kMaxBodyBytes);
        }
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return resp;
}

std::wstring NetworkHelper::Utf8ToWString(const std::string& str)
{
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    if (size_needed <= 0) return std::wstring();
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring NetworkHelper::WithCacheBuster(const std::wstring& url)
{
    static LONG counter = 0;
    const unsigned long long unique = (static_cast<unsigned long long>(GetTickCount64()) << 16)
        ^ static_cast<unsigned long long>(InterlockedIncrement(&counter));
    return url + (url.find(L'?') == std::wstring::npos ? L"?" : L"&") + L"_=" + std::to_wstring(unique);
}
