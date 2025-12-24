#pragma once
#include <string>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

class NetworkClient {
public:
  NetworkClient();
  ~NetworkClient();

  // Sends a POST request to the specified URL with JSON body.
  // Returns the response body as a string.
  std::string Post(const std::wstring &domain, const std::wstring &path,
                   const std::string &jsonBody,
                   const std::wstring &extraHeaders = L"");

  std::string Get(const std::wstring &domain, const std::wstring &path,
                  const std::string &authHeaderValue = "");

private:
  HINTERNET hSession;
  HINTERNET hConnect;
  HINTERNET hRequest;
};
