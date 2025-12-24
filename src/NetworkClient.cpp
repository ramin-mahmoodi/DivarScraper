#include "NetworkClient.h"
#include <iostream>
#include <vector>

NetworkClient::NetworkClient() {
  hSession = NULL;
  hConnect = NULL;
  hRequest = NULL;
}

NetworkClient::~NetworkClient() {
  if (hRequest)
    WinHttpCloseHandle(hRequest);
  if (hConnect)
    WinHttpCloseHandle(hConnect);
  if (hSession)
    WinHttpCloseHandle(hSession);
}

std::string
NetworkClient::Post(const std::wstring &domain, const std::wstring &path,
                    const std::string &jsonBody,
                    const std::wstring &extraHeaders) { // Add headers arg
  std::string response = "";

  hSession = WinHttpOpen(L"DivarScraper/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return "";

  hConnect =
      WinHttpConnect(hSession, domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return "";
  }

  hRequest = WinHttpOpenRequest(
      hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return "";
  }

  std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; "
                         L"x64)\r\nContent-Type: application/json\r\n";
  if (!extraHeaders.empty())
    headers += extraHeaders;

  bool bResults = WinHttpSendRequest(hRequest, headers.c_str(),
                                     headers.length(), (LPVOID)jsonBody.c_str(),
                                     jsonBody.length(), jsonBody.length(), 0);

  if (bResults) {
    bResults = WinHttpReceiveResponse(hRequest, NULL);
  }

  if (bResults) {
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer;
    do {
      dwSize = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
        break;
      if (!dwSize)
        break;
      pszOutBuffer = new char[dwSize + 1];
      if (!pszOutBuffer) {
        break;
      }
      ZeroMemory(pszOutBuffer, dwSize + 1);
      if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize,
                          &dwDownloaded)) {
        response.append(pszOutBuffer, dwDownloaded);
      }
      delete[] pszOutBuffer;
    } while (dwSize > 0);
  }

  if (hRequest)
    WinHttpCloseHandle(hRequest);
  if (hConnect)
    WinHttpCloseHandle(hConnect);
  if (hSession)
    WinHttpCloseHandle(hSession);

  hRequest = NULL;
  hConnect = NULL;
  hSession = NULL;

  return response;
}

std::string NetworkClient::Get(const std::wstring &domain,
                               const std::wstring &path,
                               const std::string &authHeader) {
  std::string response = "";
  HINTERNET hSession =
      WinHttpOpen(L"DivarScraper/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return "";

  HINTERNET hConnect =
      WinHttpConnect(hSession, domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return "";
  }

  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return "";
  }

  std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; "
                         L"x64)\r\nContent-Type: application/json\r\n";
  std::wstring wAuth(authHeader.begin(), authHeader.end());
  headers += L"Authorization: " + wAuth + L"\r\n";

  bool bResults =
      WinHttpSendRequest(hRequest, headers.c_str(), headers.length(),
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

  if (bResults)
    bResults = WinHttpReceiveResponse(hRequest, NULL);

  if (bResults) {
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer;
    do {
      dwSize = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
        break;
      if (!dwSize)
        break;
      pszOutBuffer = new char[dwSize + 1];
      if (!pszOutBuffer)
        break;
      ZeroMemory(pszOutBuffer, dwSize + 1);
      if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize,
                          &dwDownloaded))
        response.append(pszOutBuffer, dwDownloaded);
      delete[] pszOutBuffer;
    } while (dwSize > 0);
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return response;
}
