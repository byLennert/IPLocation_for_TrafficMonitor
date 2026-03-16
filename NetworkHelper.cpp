#include "NetworkHelper.h"
#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <string>

#pragma comment(lib, "winhttp.lib")

HttpResponse NetworkHelper::HttpGet(const std::wstring& url, int timeout_ms)
{
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    HttpResponse resp;
    
    // Parse URL
    URL_COMPONENTS urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    
    // Allocate buffers for URL parts
    wchar_t szHostName[256];
    wchar_t szUrlPath[1024];
    
    urlComp.lpszHostName = szHostName;
    urlComp.dwHostNameLength = ARRAYSIZE(szHostName);
    urlComp.lpszUrlPath = szUrlPath;
    urlComp.dwUrlPathLength = ARRAYSIZE(szUrlPath);
    urlComp.dwSchemeLength = 1; // Set to non-zero so we can check nScheme

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp))
    {
        return resp;
    }

    hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0 Safari/537.36",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (hSession)
    {
        WinHttpSetTimeouts(hSession, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

        WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxy{};
        if (WinHttpGetIEProxyConfigForCurrentUser(&ieProxy))
        {
            if (ieProxy.lpszProxy && *ieProxy.lpszProxy)
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

        hConnect = WinHttpConnect(hSession, szHostName, urlComp.nPort, 0);
    }

    if (hConnect)
    {
        hRequest = WinHttpOpenRequest(hConnect, L"GET", szUrlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
            (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    }

    if (hRequest)
    {
        std::wstring headers = L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
            L"Accept-Language: zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
            L"Connection: Keep-Alive\r\n";
        
        BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

        if (bResults)
        {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        if (bResults)
        {
            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
            {
                resp.status_code = statusCode;
            }

            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do
            {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                    break;
                
                if (dwSize == 0)
                    break;

                std::vector<char> buffer(dwSize + 1);
                if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded))
                {
                    resp.body.append(buffer.data(), dwDownloaded);
                }
            } while (dwSize > 0);
        }
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return resp;
}

std::wstring NetworkHelper::Utf8ToWString(const std::string& str)
{
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
