#include "CategoryDB.h" // Generated Categories
#include "CityDB.h"     // City list with IDs
#include "Database.h"
#include "HeaderBar.h"
#include "LayoutEngine.h"
#include "NetworkClient.h"
#include <atomic>
#include <codecvt>
#include <commctrl.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <fstream>
#include <locale>
#include <regex>
#include <rpc.h>
#include <set>
#include <shlwapi.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <wincodec.h>
#include <windows.h>

#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "ole32.lib")

using namespace D2D1;

// --- ID Definitions ---
#define ID_BTN_LOGIN 101
#define ID_EDIT_PHONE 102
#define ID_BTN_VERIFY 103
#define ID_EDIT_CODE 104
#define ID_BTN_SCRAPE 106
#define ID_BTN_STOP 107
#define ID_BTN_EXPORT 108
#define ID_LIST_ADS 109
#define ID_EDIT_LOG 110
#define ID_COMBO_CITY 111
#define ID_COMBO_CAT 112
#define ID_LBL_STATS 113
#define ID_BTN_LOGOUT 114
#define ID_EDIT_PAGES 115

#define WM_LOGIN_RESULT 0x0401

// Divar Brand Colors (RGB format for D2D1::ColorF)
#define COL_DIVAR_RED_NORMAL 0xA62626 // Dark Red
#define COL_DIVAR_RED_HOVER 0xBE2626  // Lighter Red
#define COL_DIVAR_BLACK 0x101010      // True Black
#define COL_DIVAR_GRAY 0x6B7280       // Tailwind gray-500
#define COL_BORDER 0xD1D5DB           // Tailwind gray-300
#define COL_BG_GRAY 0xF3F4F6          // Tailwind gray-100

// --- Global Resources ---
HINSTANCE hInst;
HWND hMainWnd;
HWND hPhoneEdit, hCodeEdit, hPlaceholder;
HWND hListAds, hLogbox, hBtnScrape, hBtnStop, hBtnExport, hBtnLogout,
    hComboCity, hComboCat, hLblStats, hEditPages, hLabCity, hLabCat, hLabPages;
HWND hNavbar = NULL; // Navbar panel (D2D rendered child window)

// D2D Resources
ID2D1Factory3 *pD2DFactory = nullptr;
ID2D1HwndRenderTarget *pRenderTarget = nullptr;
ID2D1SvgDocument *pSvgDoc = nullptr;
IDWriteFactory *pDWriteFactory = nullptr;
IDWriteTextFormat *pTextFormatTitle = nullptr;
IDWriteTextFormat *pTextFormatBody = nullptr;
IDWriteTextFormat *pTextFormatButton = nullptr;

ID2D1SolidColorBrush *pBrushRed = nullptr;
ID2D1SolidColorBrush *pBrushBlack = nullptr;
ID2D1SolidColorBrush *pBrushGray = nullptr;
ID2D1SolidColorBrush *pBrushBorder = nullptr;
ID2D1SolidColorBrush *pBrushWhite = nullptr;

NetworkClient g_Net;
Database g_Db("divar_data.db");
LayoutEngine g_Layout;

std::string g_userPhone;
std::string g_token;
std::string g_accessToken;
std::string g_sessionToken;
enum AppState { STATE_LOGIN_PHONE, STATE_LOGIN_CODE, STATE_LOGGED_IN };

AppState g_State = STATE_LOGIN_PHONE;
std::wstring g_InputError; // Error message for Input field

// Animation State
bool g_IsHovered = false;
bool g_IsPressed = false;
bool g_IsLoading = false;
float g_AnimProgress = 0.0f;
int g_selectedCityId = 715;              // Default: All Iran
std::string g_selectedCategorySlug = ""; // All Categories
float g_SpinnerAngle = 0.0f;
const float ANIM_SPEED = 0.1f;

std::atomic<bool> g_ScrapingRunning(false);
std::atomic<bool> g_StopRequested(false);
std::atomic<int> g_StatsAdsFound(0);
std::atomic<int> g_StatsPhonesSaved(0);
std::thread g_ScrapeThread;

struct LoginResult {
  bool success;
  std::string message;
  std::string token;
  std::string accessToken;
};

// --- Helpers ---
template <class Interface>
inline void SafeRelease(Interface **ppInterfaceToRelease) {
  if (*ppInterfaceToRelease != NULL) {
    (*ppInterfaceToRelease)->Release();
    (*ppInterfaceToRelease) = NULL;
  }
}

std::wstring s2ws(const std::string &s) {
  if (s.empty())
    return L"";
  int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, 0, 0);
  if (len == 0)
    return L"";
  std::vector<wchar_t> buf(len);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, buf.data(), len);
  return std::wstring(buf.begin(), buf.end() - 1);
}

std::string GetJsonValue(const std::string &json, const std::string &key) {
  std::string keyPattern = "\"" + key + "\"";
  size_t keyPos = json.find(keyPattern);
  if (keyPos == std::string::npos)
    return "";
  size_t colonPos = json.find(":", keyPos + keyPattern.length());
  if (colonPos == std::string::npos)
    return "";
  size_t valueStart = json.find("\"", colonPos + 1);
  if (valueStart == std::string::npos)
    return "";
  size_t valueEnd = json.find("\"", valueStart + 1);
  if (valueEnd == std::string::npos)
    return "";
  return json.substr(valueStart + 1, valueEnd - valueStart - 1);
}

// --- DPI System ---
UINT GetWindowDPI(HWND hWnd) {
  HMODULE hUser32 = GetModuleHandle(L"user32.dll");
  typedef UINT(WINAPI * GetDpiForWindowProc)(HWND);
  GetDpiForWindowProc getDpiForWindow =
      (GetDpiForWindowProc)GetProcAddress(hUser32, "GetDpiForWindow");
  if (getDpiForWindow)
    return getDpiForWindow(hWnd);
  HDC hdc = GetDC(hWnd);
  UINT dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  ReleaseDC(hWnd, hdc);
  return dpi;
}

int DipToPx(int dip, UINT dpi) { return MulDiv(dip, dpi, 96); }

void CreateAppFonts(UINT dpi, float scaleFactor) {
  SafeRelease(&pTextFormatTitle);
  SafeRelease(&pTextFormatBody);
  SafeRelease(&pTextFormatButton);

  if (pDWriteFactory) {
    pDWriteFactory->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"fa-IR", &pTextFormatTitle);
    pDWriteFactory->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"fa-IR", &pTextFormatBody);
    pDWriteFactory->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"fa-IR", &pTextFormatButton);

    if (pTextFormatTitle) {
      pTextFormatTitle->SetTextAlignment(
          DWRITE_TEXT_ALIGNMENT_LEADING); // LEADING = RIGHT in RTL
      pTextFormatTitle->SetReadingDirection(
          DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    }
    if (pTextFormatBody) {
      pTextFormatBody->SetTextAlignment(
          DWRITE_TEXT_ALIGNMENT_LEADING); // LEADING = RIGHT in RTL
      pTextFormatBody->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
      pTextFormatBody->SetReadingDirection(
          DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    }
    if (pTextFormatButton) {
      pTextFormatButton->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      pTextFormatButton->SetParagraphAlignment(
          DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
  }
}

// --- Logic ---
void SaveSession(const std::string &token) {
  std::ofstream out("session.txt");
  if (out.is_open()) {
    out << g_token << "\n";
    out << g_accessToken << "\n";
    out << g_sessionToken << "\n";
    out.close();
  }
}

bool LoadSession() {
  std::ifstream in("session.txt");
  if (in.is_open()) {
    std::getline(in, g_token);
    std::getline(in, g_accessToken);
    std::getline(in, g_sessionToken);
    in.close();

    // If we have at least one token, consider it valid
    if (!g_token.empty() || !g_accessToken.empty())
      return true;
  }
  return false;
}

void ClearSession() {
  g_token = "";
  remove("session.txt");
}

void UpdateAdStatus(int index, const std::wstring &status,
                    const std::wstring &phone) {
  if (hListAds) {
    ListView_SetItemText(hListAds, index, 2, (LPWSTR)status.c_str());
    if (!phone.empty())
      ListView_SetItemText(hListAds, index, 1, (LPWSTR)phone.c_str());
  }
}

void Log(const std::wstring &msg) {
  int len = GetWindowTextLength(hLogbox);
  SendMessage(hLogbox, EM_SETSEL, (WPARAM)len, (LPARAM)len);
  SendMessage(hLogbox, EM_REPLACESEL, 0, (LPARAM)(msg + L"\r\n").c_str());
}

std::string GenerateUUID() {
  UUID uuid;
  UuidCreate(&uuid);
  unsigned char *str;
  UuidToStringA(&uuid, &str);
  std::string s((char *)str);
  RpcStringFreeA(&str);
  return s;
}

void ScraperWorker(int cityId, int pages, std::string cat) {
  g_ScrapingRunning = true;
  g_StopRequested = false;
  g_StatsAdsFound = 0;
  g_StatsPhonesSaved = 0;

  Log(L"Scraping Started...");

  for (int p = 0; p < pages && !g_StopRequested; p++) {
    Log(L"Fetching Page " + std::to_wstring(p + 1));

    // Use real category or search query if needed.
    // For now implementing basic search logic
    std::string payload;

    if (cat.empty() || cat == "root") {
      // Basic Search (Legacy)
      if (p == 0) {
        payload = "{\"city_ids\":[\"" + std::to_string(cityId) +
                  "\"],\"search_data\":{\"form_ids\":[\"basic-search\"]}}";
      } else {
        payload = "{\"city_ids\":[\"" + std::to_string(cityId) +
                  "\"],\"pagination_data\":{\"page\":" + std::to_string(p) +
                  "},\"search_data\":{\"form_ids\":[\"basic-search\"]}}";
      }
    } else {
      // Category Search
      std::string search_data_json =
          "{\"form_data\":{\"data\":{\"category\":{\"str\":{\"value\":\"" +
          cat + "\"}}}}}";
      std::string place_hash = std::to_string(cityId) + "||" + cat;

      std::string pagination = "";
      if (p > 0) {
        pagination = ",\"pagination_data\":{\"@type\":\"type.googleapis.com/"
                     "post_list.PaginationData\",\"page\":" +
                     std::to_string(p) +
                     ",\"layer_page\":" + std::to_string(p) + "}";
      }

      payload = "{\"city_ids\":[\"" + std::to_string(cityId) +
                "\"],\"source_view\":\"CATEGORY\"" + pagination +
                ",\"search_data\":" + search_data_json + ",\"place_hash\":\"" +
                place_hash + "\"}";
    }

    std::string resp =
        g_Net.Post(L"api.divar.ir", L"/v8/postlist/w/search", payload);

    // Debug Log
    if (resp.length() > 0) {
      Log(L"Resp Len: " + std::to_wstring(resp.length()));
      Log(L"Resp Start: " + s2ws(resp.substr(0, 200)));
    } else {
      Log(L"Resp Empty!");
    }

    size_t pos = 0;
    int count = 0;
    // Robust parsing: find "token", then colon, then value quotes
    while ((pos = resp.find("\"token\"", pos)) != std::string::npos &&
           !g_StopRequested) {
      size_t colon = resp.find(":", pos);
      if (colon == std::string::npos) {
        pos += 5;
        continue;
      }

      size_t startQ = resp.find("\"", colon);
      if (startQ == std::string::npos) {
        pos = colon + 1;
        continue;
      }

      size_t endQ = resp.find("\"", startQ + 1);
      if (endQ == std::string::npos)
        break; // Should not happen if valid JSON

      std::string token = resp.substr(startQ + 1, endQ - startQ - 1);
      pos = endQ; // Advance pos

      // Check for duplicates
      static std::set<std::string> seenTokens;
      if (p == 0 && count == 0)
        seenTokens.clear(); // Clear on start of new scrape (approximate, better
                            // to clear at function start)

      if (seenTokens.find(token) != seenTokens.end()) {
        continue;
      }
      seenTokens.insert(token);

      // Add to list
      LVITEM lvi = {0};
      lvi.mask = LVIF_TEXT;
      lvi.pszText = (LPWSTR)s2ws(token).c_str();
      lvi.iItem = g_StatsAdsFound;
      ListView_InsertItem(hListAds, &lvi);
      UpdateAdStatus(g_StatsAdsFound, L"Waiting...", L"");

      // Get Contact
      std::string contactUrl = "/v8/postcontact/web/contact_info_v2/" + token;
      std::string uuid = GenerateUUID();
      std::string contactPayload = "{\"contact_uuid\":\"" + uuid + "\"}";

      // Use Bearer Token (JWT) + POST
      std::wstring authH =
          L"Authorization: Bearer " + s2ws(g_accessToken) + L"\r\n";

      std::string contactResp =
          g_Net.Post(L"api.divar.ir", s2ws(contactUrl), contactPayload, authH);

      // Debug Contact Resp
      if (contactResp.length() > 0) {
        Log(L"Contact Resp: " + s2ws(contactResp.substr(0, 150)));
      } else {
        Log(L"Contact Resp Empty");
      }

      // Extract phone using Regex (looks for "09" followed by 9 digits)
      std::string phone = "";
      try {
        std::regex phone_pattern("09[0-9]{9}");
        std::smatch match;
        if (std::regex_search(contactResp, match, phone_pattern)) {
          phone = match.str(0);
        }
      } catch (...) {
        Log(L"Regex Error");
      }

      if (!phone.empty()) {
        UpdateAdStatus(g_StatsAdsFound, L"Done", s2ws(phone));
        // Fix: Pass token as URL to satisfy UNIQUE constraint and allow saving
        // multiple numbers
        g_Db.saveNumber(phone, "", token);
        g_StatsPhonesSaved++;
      } else {
        UpdateAdStatus(g_StatsAdsFound, L"No Access / Error", L"");
      }

      g_StatsAdsFound++;
      count++;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(5000)); // Rate limit 5s for CAPTCHA
    }

    if (count == 0)
      Log(L"No ads found on this page.");
  }

  g_ScrapingRunning = false;
  Log(L"Scraping Finished.");
  EnableWindow(hBtnScrape, TRUE);
  EnableWindow(hBtnStop, FALSE);
}

// --- Layout System (Compact, matching Clone) ---
const float L_PAD = 32.0f;            // p-8 = 32px padding
const float L_CARD_W = 420.0f;        // Slightly narrower than max-w-md
const float L_CARD_H = 320.0f;        // Compact height
const float L_INPUT_PAD_X = 32.0f;    // Side padding
const float L_INPUT_Y = 160.0f;       // Input Y position after header
const float L_INPUT_H = 48.0f;        // h-12 = 48px
const float L_BTN_PAD_X = 32.0f;      // Button side padding
const float L_BTN_BOTTOM_PAD = 32.0f; // Bottom padding
const float L_BTN_H = 48.0f;          // h-12 = 48px

D2D1_RECT_F GetCardRect(float windowLogicalW, float windowLogicalH) {
  return D2D1::RectF(
      (windowLogicalW - L_CARD_W) / 2.0f, (windowLogicalH - L_CARD_H) / 2.0f,
      (windowLogicalW + L_CARD_W) / 2.0f, (windowLogicalH + L_CARD_H) / 2.0f);
}

D2D1_RECT_F GetInputRect(D2D1_RECT_F card) {
  return D2D1::RectF(card.left + L_INPUT_PAD_X, card.top + L_INPUT_Y,
                     card.right - L_INPUT_PAD_X,
                     card.top + L_INPUT_Y + L_INPUT_H);
}

D2D1_RECT_F GetButtonRect(D2D1_RECT_F card) {
  return D2D1::RectF(card.left + L_BTN_PAD_X,
                     card.bottom - L_BTN_BOTTOM_PAD - L_BTN_H,
                     card.right - L_BTN_PAD_X, card.bottom - L_BTN_BOTTOM_PAD);
}

// --- D2D ---
void DiscardDeviceResources() {
  SafeRelease(&pSvgDoc);
  SafeRelease(&pBrushRed);
  SafeRelease(&pBrushBlack);
  SafeRelease(&pBrushGray);
  SafeRelease(&pBrushBorder);
  SafeRelease(&pBrushWhite);
  SafeRelease(&pRenderTarget);
}

HRESULT CreateDeviceResources(HWND hWnd) {
  if (pRenderTarget)
    return S_OK;

  RECT rc;
  GetClientRect(hWnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

  HRESULT hr = pD2DFactory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(hWnd, size), &pRenderTarget);

  if (SUCCEEDED(hr)) {
    UINT dpi = GetWindowDPI(hWnd);
    pRenderTarget->SetDpi((float)dpi, (float)dpi);

    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.65f, 0.15f, 0.15f),
                                         &pBrushRed); // Divar Red
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f),
                                         &pBrushBlack); // TRUE BLACK
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.42f, 0.45f, 0.50f),
                                         &pBrushGray); // Tailwind gray-500
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.82f, 0.84f, 0.86f),
                                         &pBrushBorder); // Light gray border
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                         &pBrushWhite);

    IStream *pStream = NULL;
    if (SUCCEEDED(SHCreateStreamOnFile(L"Logo.svg", STGM_READ, &pStream))) {
      ID2D1DeviceContext5 *pDC = nullptr;
      if (SUCCEEDED(pRenderTarget->QueryInterface(&pDC))) {
        pDC->CreateSvgDocument(pStream, D2D1::SizeF(0, 0), &pSvgDoc);
        pDC->Release();
      }
      pStream->Release();
    }
  }
  return hr;
}

D2D1_COLOR_F LerpColor(D2D1_COLOR_F c1, D2D1_COLOR_F c2, float t) {
  return D2D1::ColorF(c1.r + (c2.r - c1.r) * t, c1.g + (c2.g - c1.g) * t,
                      c1.b + (c2.b - c1.b) * t, 1.0f);
}

void OnPaintDashboard(HWND hWnd); // Forward declaration

void OnPaint(HWND hWnd) {
  if (g_State == STATE_LOGGED_IN) {
    OnPaintDashboard(hWnd);
    return;
  }
  if (FAILED(CreateDeviceResources(hWnd)))
    return;

  UINT dpi = GetWindowDPI(hWnd);
  RECT rc;
  GetClientRect(hWnd, &rc);
  pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
  pRenderTarget->SetDpi((float)dpi, (float)dpi);

  pRenderTarget->BeginDraw();
  pRenderTarget->Clear(D2D1::ColorF(COL_BG_GRAY));

  float logicalW = (float)(rc.right - rc.left) * 96.0f / dpi;
  float logicalH = (float)(rc.bottom - rc.top) * 96.0f / dpi;

  D2D1_RECT_F card = GetCardRect(logicalW, logicalH);
  pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(card, 8, 8),
                                      pBrushWhite);
  pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(card, 8, 8),
                                      pBrushBorder, 1.0f);

  if (pSvgDoc) {
    ID2D1DeviceContext5 *pDC = nullptr;
    if (SUCCEEDED(pRenderTarget->QueryInterface(&pDC))) {
      float logoW = 100.0f;
      float iconX = card.left + (L_CARD_W - logoW) / 2.0f;
      float iconY = card.top + 40.0f;
      D2D1_MATRIX_3X2_F old;
      pDC->GetTransform(&old);
      pDC->SetTransform(D2D1::Matrix3x2F::Translation(iconX, iconY));
      pDC->DrawSvgDocument(pSvgDoc);
      pDC->SetTransform(old);
      pDC->Release();
    }
  }

  // Title: 32px from top and sides, 8px margin-bottom (mb-2)
  D2D1_RECT_F titleR =
      D2D1::RectF(card.left + L_PAD, card.top + L_PAD, card.right - L_PAD,
                  card.top + L_PAD + 30.0f);
  std::wstring title = (g_State == STATE_LOGIN_PHONE)
                           ? L"ورود به حساب کاربری"
                           : L"کد تأیید را وارد کنید";
  pRenderTarget->DrawText(title.c_str(), (UINT32)title.length(),
                          pTextFormatTitle, titleR, pBrushBlack);

  // Body: 8px below title (mb-2), 32px margin-bottom (mb-8 for header block)
  D2D1_RECT_F bodyR = D2D1::RectF(card.left + L_PAD, card.top + L_PAD + 38.0f,
                                  card.right - L_PAD, card.top + L_PAD + 90.0f);
  std::wstring body = (g_State == STATE_LOGIN_PHONE)
                          ? L"برای استفاده از امکانات دیوار، لطفاً شمارهٔ موبایل "
                            L"خود را وارد کنید. کد تأیید به این شماره پیامک "
                            L"خواهد شد."
                          : L"کد پیامک‌شده به شمارهٔ " +
                                s2ws(g_userPhone) + L" را وارد کنید.";

  pRenderTarget->DrawText(body.c_str(), (UINT32)body.length(), pTextFormatBody,
                          bodyR, pBrushGray);

  // Draw Input Label
  D2D1_RECT_F labelR =
      D2D1::RectF(card.left + L_INPUT_PAD_X, card.top + L_INPUT_Y - 25.0f,
                  card.right - L_INPUT_PAD_X, card.top + L_INPUT_Y);
  std::wstring label =
      (g_State == STATE_LOGIN_PHONE) ? L"شمارهٔ موبایل" : L"کد تأیید";
  pRenderTarget->DrawText(label.c_str(), (UINT32)label.length(),
                          pTextFormatBody, labelR, pBrushGray);

  D2D1_RECT_F inputR = GetInputRect(card);

  // Update D2D input bounds for current layout
  RECT inputBounds = {(LONG)inputR.left, (LONG)inputR.top, (LONG)inputR.right,
                      (LONG)inputR.bottom};
  g_Layout.SetInputBounds(inputBounds);

  // Draw Input Border (Red if error, else Gray)
  if (!g_InputError.empty()) {
    pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(inputR, 4, 4),
                                        pBrushRed, 1.5f);
  } else {
    pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(inputR, 4, 4),
                                        pBrushBorder, 1.0f);
  }

  // Draw D2D Input (text with letter-spacing)
  g_Layout.DrawD2DInput(pRenderTarget, pDWriteFactory, pBrushBlack, pBrushGray,
                        nullptr);

  // Draw Error Text below input (if error)
  if (!g_InputError.empty()) {
    D2D1_RECT_F errR = D2D1::RectF(inputR.left, inputR.bottom + 4.0f,
                                   inputR.right, inputR.bottom + 24.0f);
    ID2D1SolidColorBrush *pBrushErr = nullptr;
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xDC2626),
                                         &pBrushErr); // Red-600
    if (pBrushErr) {
      pRenderTarget->DrawText(g_InputError.c_str(),
                              (UINT32)g_InputError.length(), pTextFormatBody,
                              errR, pBrushErr);
      pBrushErr->Release();
    }
  }

  // Terms text removed - Clone doesn't show it prominently

  D2D1_RECT_F btnR = GetButtonRect(card);

  // Animation Logic
  D2D1_COLOR_F cNormal = D2D1::ColorF(COL_DIVAR_RED_NORMAL);
  D2D1_COLOR_F cHover = D2D1::ColorF(COL_DIVAR_RED_HOVER);
  D2D1_COLOR_F cCurrent = LerpColor(cNormal, cHover, g_AnimProgress);
  if (g_IsLoading)
    cCurrent = D2D1::ColorF(COL_DIVAR_RED_HOVER); // Loading is always light red

  ID2D1SolidColorBrush *pBrushAnim = nullptr;
  pRenderTarget->CreateSolidColorBrush(cCurrent, &pBrushAnim);

  D2D1_MATRIX_3X2_F oldTr;
  pRenderTarget->GetTransform(&oldTr);

  // Scale on Click (only if not loading)
  if (g_IsPressed && !g_IsLoading) {
    float cx = btnR.left + (btnR.right - btnR.left) / 2.0f;
    float cy = btnR.top + (btnR.bottom - btnR.top) / 2.0f;
    pRenderTarget->SetTransform(
        D2D1::Matrix3x2F::Scale(0.96f, 0.96f, D2D1::Point2F(cx, cy)) * oldTr);
  }

  if (pBrushAnim) {
    pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(btnR, 4, 4),
                                        pBrushAnim);
    pBrushAnim->Release();
  } else {
    pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(btnR, 4, 4),
                                        pBrushRed);
  }

  if (g_IsLoading) {
    float cx = btnR.left + (btnR.right - btnR.left) / 2.0f;
    float cy = btnR.top + (btnR.bottom - btnR.top) / 2.0f;
    float r = 10.0f;
    ID2D1PathGeometry *pPathGeo = nullptr;
    pD2DFactory->CreatePathGeometry(&pPathGeo);
    if (pPathGeo) {
      ID2D1GeometrySink *pSink = nullptr;
      pPathGeo->Open(&pSink);
      pSink->BeginFigure(D2D1::Point2F(cx + r, cy), D2D1_FIGURE_BEGIN_HOLLOW);
      pSink->AddArc(D2D1::ArcSegment(
          D2D1::Point2F(cx, cy - r), D2D1::SizeF(r, r), 0.0f,
          D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
      pSink->EndFigure(D2D1_FIGURE_END_OPEN);
      pSink->Close();
      pSink->Release();
      pRenderTarget->SetTransform(
          D2D1::Matrix3x2F::Rotation(g_SpinnerAngle, D2D1::Point2F(cx, cy)) *
          oldTr);
      ID2D1SolidColorBrush *pWhite = nullptr;
      pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                           &pWhite);
      pRenderTarget->DrawGeometry(pPathGeo, pWhite, 2.0f);
      SafeRelease(&pWhite);
      SafeRelease(&pPathGeo);
    }
  } else {
    std::wstring btnTxt = (g_State == STATE_LOGIN_PHONE) ? L"تایید" : L"ورود";
    pRenderTarget->DrawText(btnTxt.c_str(), (UINT32)btnTxt.length(),
                            pTextFormatButton, btnR, pBrushWhite);
  }

  pRenderTarget->SetTransform(oldTr);

  if (pRenderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
    DiscardDeviceResources();
  ValidateRect(hWnd, NULL);
}

// Dashboard navbar painting with D2D
// Dashboard navbar painting with D2D
void OnPaintDashboard(HWND hWnd) {
  if (g_State != STATE_LOGGED_IN)
    return;

  // Manual header drawing removed in favor of HeaderBar class.
  // Validate rect to prevent infinite paint loop if needed,
  // though HeaderBar should handle its own area.

  RECT rc;
  GetClientRect(hWnd, &rc);
  ValidateRect(hWnd, &rc);
}

void UpdateLayout(HWND hWnd);
void InitDashboard(HWND hWnd);
void BuildLoginUI(HWND hWnd);
void OnExport();

void UpdateLayout(HWND hWnd) {
  if (g_State != STATE_LOGGED_IN) {
    InvalidateRect(hWnd, NULL, TRUE);

    UINT dpi = GetWindowDPI(hWnd);
    RECT rc;
    GetClientRect(hWnd, &rc);
    float logicalW = (float)(rc.right - rc.left) * 96.0f / dpi;
    float logicalH = (float)(rc.bottom - rc.top) * 96.0f / dpi;

    D2D1_RECT_F card = GetCardRect(logicalW, logicalH);
    D2D1_RECT_F inputR = GetInputRect(card);
    float logPad = 5.0f;

    int pxL = DipToPx((int)(inputR.left + logPad), dpi);
    int pxT = DipToPx((int)(inputR.top + logPad + 5.0f), dpi);
    int pxW = DipToPx((int)(inputR.right - inputR.left - logPad * 2), dpi);
    int pxH = DipToPx((int)(20), dpi);

    if (hPhoneEdit && g_State == STATE_LOGIN_PHONE)
      MoveWindow(hPhoneEdit, pxL, pxT, pxW, pxH, TRUE);
    if (hCodeEdit && g_State == STATE_LOGIN_CODE)
      MoveWindow(hCodeEdit, pxL, pxT, pxW, pxH, TRUE);
    if (hPlaceholder) {
      int phPadR = DipToPx(8, dpi);
      int phPadL = DipToPx(8, dpi);
      MoveWindow(hPlaceholder, pxL + phPadL, pxT, pxW - phPadR - phPadL, pxH,
                 TRUE);
    }
    return;
  }

  // ===== Dashboard layout (v10.1 Responsive) =====
  // 2-row design: 64px D2D header + dynamic content
  UINT dpi = GetWindowDPI(hWnd);
  RECT rc;
  GetClientRect(hWnd, &rc);
  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;

  // Layout constants (Responsive DPI)
  int navbarH = DipToPx(64, dpi); // HeaderBar height
  int logH = DipToPx(60, dpi);
  int margin = DipToPx(8, dpi);

  // 1. D2D Navbar (HeaderBar)
  // Ensure we move the correct window (g_HeaderBar's hwnd)
  if (g_HeaderBar && g_HeaderBar->GetHwnd()) {
    MoveWindow(g_HeaderBar->GetHwnd(), 0, 0, w, navbarH, TRUE);
    g_HeaderBar->OnSize(w, navbarH); // Update internal layout logic
  }

  // 2. Main content area (fills remaining space)
  int listY = navbarH + margin;
  int listH = h - listY - logH - margin * 2;

  if (listH < 20)
    listH = 20;

  if (hListAds)
    MoveWindow(hListAds, margin, listY, w - margin * 2, listH, TRUE);

  if (hLogbox)
    MoveWindow(hLogbox, margin, listY + listH + margin, w - margin * 2, logH,
               TRUE);
}

void InitDashboard(HWND hWnd) {
  SetWindowText(hWnd, L"DivarScraper");

  // Fixed-size layout (1200x800 window)
  // Dynamic layout
  int headerH = 64;
  int margin = 16;
  int gap = 8;
  int logH = 100;

  RECT rc;
  GetClientRect(hWnd, &rc);
  int winW = rc.right - rc.left;
  int winH = rc.bottom - rc.top;

  // ===== Custom D2D Header (React-style) =====
  if (!g_HeaderBar) {
    g_HeaderBar = new HeaderBar(hWnd);
    g_HeaderBar->Create(hInst);
  } else {
    g_HeaderBar->ClearItems();
  }

  // Right-side items (Added first, stacked Right-to-Left)

  // 1. Logo
  HeaderItem logo;
  logo.type = HeaderItemType::Logo;
  logo.text = L"دیوار";
  logo.w = 56;
  logo.alignment = HeaderAlignment::Right;
  g_HeaderBar->AddItem(logo);

  // 2. City Dropdown
  HeaderItem city;
  city.type = HeaderItemType::Dropdown;
  city.text = L"کل ایران";
  city.iconType = HeaderIcon::None;
  city.alignment = HeaderAlignment::Right;
  city.onClick = [hWnd]() {
    std::vector<MenuItem> cityMenu;
    for (const auto &c : g_Cities) {
      MenuItem mi;
      mi.text = c.name;
      mi.id = std::to_string(c.id);
      cityMenu.push_back(mi);
    }
    g_HeaderBar->ShowDropdown(1, cityMenu,
                              [](int idx, std::wstring val, std::string id) {
                                g_HeaderBar->SetItemText(1, val);
                                g_selectedCityId = std::stoi(id);
                              });
  };
  g_HeaderBar->AddItem(city);

  // 3. Category Dropdown
  HeaderItem cat;
  cat.type = HeaderItemType::Dropdown;
  cat.text = L"همهٔ دسته‌ها";
  cat.iconType = HeaderIcon::None;
  cat.alignment = HeaderAlignment::Right;
  cat.onClick = [hWnd]() {
    g_HeaderBar->ShowDropdown(2, g_MenuCategories,
                              [](int idx, std::wstring val, std::string id) {
                                g_HeaderBar->SetItemText(2, val);
                                g_selectedCategorySlug = id;
                              });
  };
  g_HeaderBar->AddItem(cat);

  // 4. Divider
  HeaderItem div;
  div.type = HeaderItemType::Divider;
  div.alignment = HeaderAlignment::Right;
  g_HeaderBar->AddItem(div);

  // 5. Start/Stop Toggle Button
  HeaderItem btnToggle;
  btnToggle.type = HeaderItemType::Button;
  btnToggle.text = L"شروع";
  btnToggle.iconType = HeaderIcon::Play;
  btnToggle.w = 90;
  btnToggle.alignment = HeaderAlignment::Right;
  btnToggle.onClick = []() {
    if (!g_HeaderBar)
      return;

    // Toggle State
    if (g_ScrapingRunning) {
      // Stop
      g_StopRequested = true;
      // UI updates when checking state or immediately?
      // Let's update UI to "Start" properties?
      // No, clicking Stop requests stop. The actual transition to "Stopped"
      // happens in thread logic or we can force UI here. Actually, existing
      // logic for "Stop" button just set flag. If we want a toggle, clicking
      // "Stop" (when running) requests stop. We should probably keep showing
      // "Stop" until it actually stops? Or change to "Stopping..."? For
      // simplicity, let's just toggle request.

      // Wait, if I click "Stop", it requests stop. When scraper finishes,
      // `g_ScrapingRunning` becomes false. But how does UI update automatically
      // if we don't have a timer checking `g_ScrapingRunning`? `main.cpp`
      // timer? We can update UI immediately here just to swap button state if
      // we assume instant effect? No, ScraperWorker sets `g_ScrapingRunning =
      // false` at end. We need a timer to Update UI? Or just update UI here
      // assuming the user wants to see change.

      // BUT, if I click "Start" (Play), I launch thread. I should change button
      // to "Stop" (Square) immediately.
    } else {
      // Start
      g_ScrapingRunning = true;
      g_StopRequested = false;
      int cityId = g_selectedCityId;
      std::string catSlug = g_selectedCategorySlug;
      g_ScrapeThread = std::thread(ScraperWorker, cityId, 5, catSlug);
      g_ScrapeThread.detach();

      // Update to "Stop" state
      g_HeaderBar->SetItemText(4, L"توقف");
      g_HeaderBar->SetItemIcon(4, HeaderIcon::Stop);
      // Color is handled by DrawButton based on text "توقف" -> Red
      g_HeaderBar->Invalidate();
    }
  };
  g_HeaderBar->AddItem(btnToggle);

  // 7. Export Button
  HeaderItem btnExport;
  btnExport.type = HeaderItemType::Button;
  btnExport.text = L"خروجی";
  btnExport.iconType = HeaderIcon::Download;
  btnExport.w = 90;
  btnExport.alignment = HeaderAlignment::Right;
  btnExport.onClick = []() { OnExport(); };
  g_HeaderBar->AddItem(btnExport);

  // Left-side items

  // 8. User Area
  HeaderItem user;
  user.type = HeaderItemType::UserArea;
  user.text = L"دیوار من";
  user.iconType = HeaderIcon::User;
  user.w = 120;
  user.alignment = HeaderAlignment::Left;
  user.onClick = [hWnd]() {
    std::vector<MenuItem> userMenu;

    MenuItem title;
    title.text = L"حساب کاربری";
    title.isTitle = true;
    userMenu.push_back(title);

    MenuItem logout;
    logout.text = L"خروج";
    logout.id = "logout";
    // logout.iconType = HeaderIcon::Logout;  // Temporarily disabled
    userMenu.push_back(logout);

    // Show dropdown under the UserArea item (Align Right because we want it to
    // appear under "My Divar" which is on the left? No, "My Divar" is aligned
    // LEFT in HeaderBar, but in RTL layout, left is actually logically 'start'
    // or 'end'? Wait, HeaderAlignment::Left means it's on the left side of the
    // screen.
    g_HeaderBar->ShowDropdown(
        6, userMenu,
        [hWnd](int idx, std::wstring val, std::string id) {
          if (id == "logout") {
            ClearSession();
            g_State = STATE_LOGIN_PHONE;
            if (g_HeaderBar) {
              delete g_HeaderBar;
              g_HeaderBar = nullptr;
            }
            if (hListAds) {
              DestroyWindow(hListAds);
              hListAds = NULL;
            }
            if (hLogbox) {
              DestroyWindow(hLogbox);
              hLogbox = NULL;
            }
            BuildLoginUI(hMainWnd);
          }
        },
        true); // Use alignRight = true if we want the menu's right edge to
               // align with the button's right edge? Actually, for a
               // left-aligned button, we probably want menu's left edge to
               // align. But HeaderPopup::Show with alignRight=true subtracts
               // width from X. Let's see.
  };
  g_HeaderBar->AddItem(user);

  // Initial Update
  g_HeaderBar->OnSize(winW, headerH);
  // Note: OnSize calls UpdateLayout internally.

  // ===== Main content area =====
  DWORD s = WS_VISIBLE | WS_CHILD;
  int contentTop = headerH + margin;
  int listH = winH - headerH - logH - margin * 3;

  hListAds = CreateWindowEx(
      WS_EX_LAYOUTRTL, WC_LISTVIEW, L"", s | WS_BORDER | LVS_REPORT, margin,
      contentTop, winW - margin * 2, listH, hWnd, (HMENU)ID_LIST_ADS, hInst, 0);
  LVCOLUMN lvc = {LVCF_TEXT | LVCF_WIDTH, 0, 350, (LPWSTR)L"لینک آگهی"};
  ListView_InsertColumn(hListAds, 0, &lvc);
  lvc.cx = 200;
  lvc.pszText = (LPWSTR)L"شماره موبایل";
  ListView_InsertColumn(hListAds, 1, &lvc);
  lvc.cx = 150;
  lvc.pszText = (LPWSTR)L"وضعیت";
  ListView_InsertColumn(hListAds, 2, &lvc);

  hLogbox = CreateWindow(
      L"EDIT", L"", s | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
      margin, contentTop + listH + margin, winW - margin * 2, logH, hWnd,
      (HMENU)ID_EDIT_LOG, hInst, 0);

  // Set font for content area
  // Set font for content area - Larger size for readability
  // Set font for content area - Larger size for readability (Updated to 34)
  HFONT hFont =
      CreateFont(34, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                 DEFAULT_PITCH, L"Segoe UI");
  SendMessage(hListAds, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessage(hLogbox, WM_SETFONT, (WPARAM)hFont, TRUE);

  // Monitor scraping state
  SetTimer(hWnd, 4, 200, NULL);
}

void BuildLoginUI(HWND hWnd) {
  UINT dpi = GetWindowDPI(hWnd);
  SetWindowText(hWnd, L"DivarScraper");

  // Destroy all child windows
  HWND hChild = GetWindow(hWnd, GW_CHILD);
  while (hChild) {
    HWND n = GetWindow(hChild, GW_HWNDNEXT);
    // Don't destroy HeaderBar if we want to reuse it?
    // Usually Login UI doesn't have HeaderBar.
    DestroyWindow(hChild);
    hChild = n;
  }
  hListAds = NULL;
  hLogbox = NULL;
  hComboCity = NULL;
  hPhoneEdit = NULL;
  hCodeEdit = NULL;
  hPlaceholder = NULL;

  // Also HeaderBar window is destroyed if it was a child.
  // So g_HeaderBar pointer becomes invalid HWND but object remains?
  // HeaderBar destructor calls DestroyWindow.
  // If we destroyed it manually via GetWindow loop, we should reset pointer?
  // Or better, let's not worry about pointer validity here as InitDashboard
  // re-creates it.
  if (g_HeaderBar) {
    delete g_HeaderBar;
    g_HeaderBar = nullptr;
  }

  // Get input rect for D2D input
  RECT rc;
  GetClientRect(hWnd, &rc);
  float logicalW = (float)(rc.right - rc.left) * 96.0f / dpi;
  float logicalH = (float)(rc.bottom - rc.top) * 96.0f / dpi;
  D2D1_RECT_F card = GetCardRect(logicalW, logicalH);
  D2D1_RECT_F inputR = GetInputRect(card);

  // Create D2D input with letter-spacing
  LPCWSTR placeholderText =
      (g_State == STATE_LOGIN_PHONE) ? L"مثال: ۰۹۱۲۳۴۵۶۷۸۹" : L"کد تأیید";
  int maxLen = (g_State == STATE_LOGIN_PHONE) ? 11 : 6;

  RECT inputBounds = {(LONG)inputR.left, (LONG)inputR.top, (LONG)inputR.right,
                      (LONG)inputR.bottom};

  // Letter spacing of 4 DIP for tracking-widest effect
  g_Layout.CreateD2DInput(inputBounds, 4.0f, 18.0f, placeholderText, maxLen,
                          true);

  // Start cursor blink timer
  SetTimer(hWnd, 3, 500, NULL); // Timer ID 3 for cursor blink

  UpdateLayout(hWnd);
}

void ThreadLoginPhone(std::string phone, HWND hWnd) {
  std::string body = "{\"phone\":\"" + phone + "\"}";
  LoginResult *res = new LoginResult();
  std::string resp =
      g_Net.Post(L"api.divar.ir", L"/v5/auth/authenticate", body);

  if (resp.find("AUTHENTICATION_VERIFICATION_CODE_SENT") != std::string::npos ||
      resp.find("authenticate_response") != std::string::npos) {
    res->success = true;
  } else {
    res->success = false;
    res->message = "Error: " + resp.substr(0, 100);
  }
  PostMessage(hWnd, WM_LOGIN_RESULT, (WPARAM)STATE_LOGIN_PHONE, (LPARAM)res);
}

void ThreadLoginCode(std::string phone, std::string code, HWND hWnd) {
  std::string body = "{\"phone\":\"" + phone + "\",\"code\":\"" + code + "\"}";
  LoginResult *res = new LoginResult();
  std::string resp = g_Net.Post(L"api.divar.ir", L"/v5/auth/confirm", body);
  if (resp.find("\"type\":\"BAD_REQUEST\"") != std::string::npos) {
    res->success = false;
    res->message = "Invalid Code";
  } else {
    std::string acc = GetJsonValue(resp, "access_token");
    if (!acc.empty()) {
      res->success = true;
      res->accessToken = acc;
      res->token = GetJsonValue(resp, "token");
    } else {
      res->success = false;
      res->message = "Login Failed: " + resp.substr(0, 50);
    }
  }
  PostMessage(hWnd, WM_LOGIN_RESULT, (WPARAM)STATE_LOGIN_CODE, (LPARAM)res);
}

// ... existing code ...

// Note: OnPaintDashboard implementation is removed or emptied?
// You requested to remove manual drawing.
// I can't overwrite it here easily since it's above InitDashboard in the file
// structure? Wait, OnPaintDashboard was at line ~680. InitDashboard is at ~848.
// I cannot replace OnPaintDashboard from here unless I include it in the
// replacement range. My replacement range is 848 to 1638. This includes
// InitDashboard, BuildLoginUI, Threads, and Main. It does NOT include
// OnPaintDashboard. I must make a valid replacement. Let's check where
// OnPaintDashboard is used. It is called in OnPaint (line 509). It is defined
// at line 689. I should probably edit OnPaintDashboard separately or extend the
// range.

void ProcessLoginStep(HWND hWnd) {
  if (g_IsLoading)
    return;
  if (g_State == STATE_LOGIN_PHONE) {
    // Get text from D2D input
    std::wstring wtext = g_Layout.GetInputText();
    // Convert wstring to string (ASCII numbers only)
    g_userPhone.clear();
    for (wchar_t wc : wtext) {
      if (wc >= L'0' && wc <= L'9') {
        g_userPhone += (char)wc;
      }
    }

    // Regex Check: ^09[0-9]{9}$
    bool valid = true;
    if (g_userPhone.length() != 11)
      valid = false;
    if (valid && g_userPhone.substr(0, 2) != "09")
      valid = false;
    if (valid) {
      for (char c : g_userPhone) {
        if (c < '0' || c > '9') {
          valid = false;
          break;
        }
      }
    }

    if (!valid) {
      g_InputError = L"شمارهٔ موبایل نادرست است. لطفا شماره را با ۰۹ شروع کنید.";
      InvalidateRect(hWnd, NULL, FALSE);
      return;
    }
    g_InputError = L""; // Clear error
    g_IsLoading = true;
    SetTimer(hWnd, 2, 16, NULL);
    InvalidateRect(hWnd, NULL, FALSE);
    std::thread(ThreadLoginPhone, g_userPhone, hWnd).detach();
  } else if (g_State == STATE_LOGIN_CODE) {
    // Get code from D2D input
    std::wstring wtext = g_Layout.GetInputText();
    std::string code;
    for (wchar_t wc : wtext) {
      if (wc >= L'0' && wc <= L'9') {
        code += (char)wc;
      }
    }
    g_IsLoading = true;
    SetTimer(hWnd, 2, 16, NULL);
    InvalidateRect(hWnd, NULL, FALSE);
    std::thread(ThreadLoginCode, g_userPhone, code, hWnd).detach();
  }
}

void OnExport() {
  if (g_Db.exportToCSV("scrap_results.csv")) {
    MessageBox(hMainWnd, L"Results exported to scrap_results.csv", L"Export",
               MB_OK);
  } else {
    MessageBox(hMainWnd, L"Export Failed!", L"Error", MB_ICONERROR);
  }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  switch (message) {
  case WM_CREATE: {
    CreateAppFonts(GetWindowDPI(hWnd), 1.0f);
    if (LoadSession()) {
      g_State = STATE_LOGGED_IN;
      InitDashboard(hWnd);
    } else
      BuildLoginUI(hWnd);
    return 0;
  }
  case WM_LOGIN_RESULT: {
    LoginResult *res = (LoginResult *)lParam;
    g_IsLoading = false;
    KillTimer(hWnd, 2);
    InvalidateRect(hWnd, NULL, FALSE);
    if (res->success) {
      if (wParam == STATE_LOGIN_PHONE) {
        g_State = STATE_LOGIN_CODE;
        BuildLoginUI(hWnd);
      } else if (wParam == STATE_LOGIN_CODE) {
        g_token = res->accessToken;
        g_accessToken = res->accessToken;
        g_sessionToken = res->token;
        SaveSession(g_token);
        g_State = STATE_LOGGED_IN;
        HWND hChild = GetWindow(hWnd, GW_CHILD);
        while (hChild) {
          HWND n = GetWindow(hChild, GW_HWNDNEXT);
          DestroyWindow(hChild);
          hChild = n;
        }
        if (pD2DFactory)
          DiscardDeviceResources();
        InitDashboard(hWnd);
      }
    } else {
      std::wstring msg = s2ws(res->message);
      MessageBox(hWnd, msg.c_str(), L"Error", MB_OK);
    }
    delete res;
    return 0;
  }
  case WM_DPICHANGED: {
    RECT *prc = (RECT *)lParam;
    SetWindowPos(hWnd, NULL, prc->left, prc->top, prc->right - prc->left,
                 prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
    UINT newDpi = HIWORD(wParam);
    CreateAppFonts(newDpi, 1.0f);
    if (g_State != STATE_LOGGED_IN)
      BuildLoginUI(hWnd);
    else
      InitDashboard(hWnd);

    g_Layout.UpdateDPI(newDpi, GetDpiForWindow(hWnd));
    return 0;
  }
  case WM_SETCURSOR: {
    if (g_State != STATE_LOGGED_IN) {
      if (g_IsHovered && !g_IsLoading) {
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return TRUE;
      }
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hWnd, &pt);
      UINT dpi = GetWindowDPI(hWnd);
      float logX = pt.x * 96.0f / dpi;
      float logY = pt.y * 96.0f / dpi;
      RECT inputBounds = g_Layout.GetInputBounds();
      POINT logPt = {(LONG)logX, (LONG)logY};
      if (PtInRect(&inputBounds, logPt)) {
        SetCursor(LoadCursor(NULL, IDC_IBEAM));
        return TRUE;
      }
    }
    break;
  }

  case WM_SIZE: {
    UpdateLayout(hWnd);
    return 0;
  }
  case WM_LBUTTONDOWN: {
    if (g_State != STATE_LOGGED_IN && !g_IsLoading) {
      if (g_IsHovered) {
        g_IsPressed = true;
        SetCapture(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
      }
      int x = LOWORD(lParam), y = HIWORD(lParam);
      UINT dpi = GetWindowDPI(hWnd);
      float logX = x * 96.0f / dpi;
      float logY = y * 96.0f / dpi;
      if (g_Layout.HandleInputClick((int)logX, (int)logY)) {
        InvalidateRect(hWnd, NULL, FALSE);
      }
    }
    return 0;
  }
  case WM_LBUTTONUP: {
    if (g_State != STATE_LOGGED_IN && !g_IsLoading) {
      if (g_IsPressed) {
        ReleaseCapture();
        g_IsPressed = false;
        if (g_IsHovered) {
          ProcessLoginStep(hWnd);
        }
        InvalidateRect(hWnd, NULL, FALSE);
      }
    }
    return 0;
  }
  case WM_MOUSEMOVE: {
    if (g_State != STATE_LOGGED_IN && !g_IsLoading) {
      int x = LOWORD(lParam), y = HIWORD(lParam);
      UINT dpi = GetWindowDPI(hWnd);
      RECT rc;
      GetClientRect(hWnd, &rc);
      float logicalW = (float)(rc.right - rc.left) * 96.0f / dpi;
      float logicalH = (float)(rc.bottom - rc.top) * 96.0f / dpi;
      float logX = x * 96.0f / dpi;
      float logY = y * 96.0f / dpi;
      D2D1_RECT_F btn = GetButtonRect(GetCardRect(logicalW, logicalH));
      bool isInside = (logX >= btn.left && logX <= btn.right &&
                       logY >= btn.top && logY <= btn.bottom);
      if (g_IsPressed) {
        if (g_IsHovered != isInside) {
          g_IsHovered = isInside;
          InvalidateRect(hWnd, NULL, FALSE);
        }
      } else {
        if (g_IsHovered != isInside) {
          g_IsHovered = isInside;
          InvalidateRect(hWnd, NULL, FALSE);
          if (isInside) {
            SetTimer(hWnd, 1, 16, NULL);
            TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0};
            TrackMouseEvent(&tme);
          }
        }
      }
    }
    return 0;
  }
  case WM_MOUSELEAVE: {
    if (g_State != STATE_LOGGED_IN) {
      if (!g_IsPressed && g_IsHovered) {
        g_IsHovered = false;
        SetTimer(hWnd, 1, 16, NULL);
        InvalidateRect(hWnd, NULL, FALSE);
      }
    }
    return 0;
  }
  case WM_CHAR: {
    if (g_State != STATE_LOGGED_IN) {
      if (g_Layout.HandleInputChar(wParam)) {
        InvalidateRect(hWnd, NULL, FALSE);
      }
      return 0;
    }
    break;
  }
  case WM_TIMER: {
    if (wParam == 1) {
      bool changed = false;
      if (g_IsHovered && g_AnimProgress < 1.0f) {
        g_AnimProgress += ANIM_SPEED;
        if (g_AnimProgress > 1.0f)
          g_AnimProgress = 1.0f;
        changed = true;
      } else if (!g_IsHovered && g_AnimProgress > 0.0f) {
        g_AnimProgress -= ANIM_SPEED;
        if (g_AnimProgress < 0.0f)
          g_AnimProgress = 0.0f;
        changed = true;
      }
      if (!changed && !g_IsHovered)
        KillTimer(hWnd, 1);
      if (changed)
        InvalidateRect(hWnd, NULL, FALSE);
    }
    if (wParam == 2 && g_IsLoading) {
      g_SpinnerAngle += 15.0f;
      if (g_SpinnerAngle >= 360.0f)
        g_SpinnerAngle -= 360.0f;
      InvalidateRect(hWnd, NULL, FALSE);
    }
    if (wParam == 3 && g_State != STATE_LOGGED_IN) {
      g_Layout.ToggleCursor();
      InvalidateRect(hWnd, NULL, FALSE);
    }
    if (wParam == 4) {
      static bool wasRunning = false;
      bool isRunning = g_ScrapingRunning;
      if (wasRunning && !isRunning) {
        // Transitioned to stopped -> Reset button
        if (g_HeaderBar) {
          g_HeaderBar->SetItemText(4, L"شروع");
          g_HeaderBar->SetItemIcon(4, HeaderIcon::Play);
          g_HeaderBar->Invalidate();
        }
      }
      wasRunning = isRunning;
    }
    return 0;
  }
  case WM_PAINT: {
    if (g_State != STATE_LOGGED_IN) {
      PAINTSTRUCT ps;
      BeginPaint(hWnd, &ps);
      OnPaint(hWnd);
      EndPaint(hWnd, &ps);
      return 0;
    }
    break;
  }
  case WM_CTLCOLORSTATIC: {
    HWND hCtl = (HWND)lParam;
    if (hCtl == hPlaceholder) {
      HDC hdc = (HDC)wParam;
      SetTextColor(hdc, RGB(107, 114, 128));
      SetBkMode(hdc, TRANSPARENT);
      return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    // Dashboard labels - white background, gray text
    if (g_State == STATE_LOGGED_IN) {
      HDC hdc = (HDC)wParam;
      SetTextColor(hdc, RGB(110, 110, 110)); // divar-gray
      SetBkMode(hdc, TRANSPARENT);
      static HBRUSH hWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));
      return (LRESULT)hWhiteBrush;
    }
    break;
  }
  case WM_DRAWITEM: {
    LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
    if (pDIS->CtlType == ODT_BUTTON) {
      HWND hBtn = pDIS->hwndItem;
      HDC hdc = pDIS->hDC;
      RECT rc = pDIS->rcItem;

      // Get button text
      wchar_t text[64] = {0};
      GetWindowTextW(hBtn, text, 64);

      // React colors
      COLORREF colBg = RGB(255, 255, 255);     // white
      COLORREF colText = RGB(110, 110, 110);   // divar-gray
      COLORREF colBorder = RGB(224, 224, 224); // divar-border

      // Hover/pressed states
      if (pDIS->itemState & ODS_SELECTED) {
        colBg = RGB(243, 244, 246); // gray-100 pressed
      } else if (pDIS->itemState & ODS_HOTLIGHT) {
        colBg = RGB(249, 250, 251); // gray-50 hover
      }

      // Special styling for specific buttons
      if (hBtn == hBtnScrape) {
        colText = RGB(34, 197, 94); // green-500 for start
      } else if (hBtn == hBtnStop) {
        colText = RGB(239, 68, 68); // red-500 for stop
      } else if (hBtn == hBtnExport) {
        colText = RGB(59, 130, 246); // blue-500 for export
      } else if (hBtn == hBtnLogout) {
        // Logout is invisible button over D2D area
        return TRUE;
      }

      // Draw background
      HBRUSH hBrush = CreateSolidBrush(colBg);
      FillRect(hdc, &rc, hBrush);
      DeleteObject(hBrush);

      // Draw subtle border
      HPEN hPen = CreatePen(PS_SOLID, 1, colBorder);
      HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
      HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
      RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
      SelectObject(hdc, hOldBrush);
      SelectObject(hdc, hOldPen);
      DeleteObject(hPen);

      // Draw text
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, colText);

      HFONT hFont =
          CreateFont(14, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
      HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
      DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(hdc, hOldFont);
      DeleteObject(hFont);

      return TRUE;
    }
    break;
  }
  case WM_COMMAND: {
    int wmId = LOWORD(wParam);
    if (HIWORD(wParam) == EN_CHANGE) {
      HWND hEdit = (g_State == STATE_LOGIN_PHONE) ? hPhoneEdit : hCodeEdit;
      if ((HWND)lParam == hEdit && hPlaceholder) {
        wchar_t buf[100] = {0};
        GetWindowTextW(hEdit, buf, 100);
        ShowWindow(hPlaceholder, (wcslen(buf) == 0) ? SW_SHOW : SW_HIDE);
      }
    }
    if (wmId == ID_BTN_LOGOUT) {
      ClearSession();
      g_State = STATE_LOGIN_PHONE;
      BuildLoginUI(hWnd);
    } else if (wmId == ID_BTN_SCRAPE) {
      /* Legacy logic removed - handled by HeaderBar
      if (!g_ScrapingRunning) {
        // ...
      }
      */
    } else if (wmId == ID_BTN_STOP) {
      g_StopRequested = true;
      EnableWindow(hBtnStop, FALSE);
    } else if (wmId == ID_BTN_EXPORT) {
      OnExport();
    }
    return 0;
  }
  case WM_DESTROY: {
    DiscardDeviceResources();
    SafeRelease(&pDWriteFactory);
    SafeRelease(&pD2DFactory);
    CoUninitialize();
    PostQuitMessage(0);
    return 0;
  }
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

// Navbar child window - D2D rendered
LRESULT CALLBACK NavbarWndProc(HWND hWnd, UINT message, WPARAM wParam,
                               LPARAM lParam) {
  switch (message) {
  case WM_COMMAND:
    return SendMessage(GetParent(hWnd), message, wParam, lParam);
  case WM_CTLCOLORSTATIC: {
    HDC hdc = (HDC)wParam;
    SetBkMode(hdc, TRANSPARENT);
    return (LRESULT)GetStockObject(WHITE_BRUSH);
  }
  case WM_PAINT: {
    if (FAILED(CreateDeviceResources(hWnd)))
      return DefWindowProc(hWnd, message, wParam, lParam);

    UINT dpi = GetWindowDPI(hWnd);
    RECT rc;
    GetClientRect(hWnd, &rc);

    pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
    pRenderTarget->SetDpi((float)dpi, (float)dpi);

    pRenderTarget->BeginDraw();

    float logicalW = (float)(rc.right - rc.left) * 96.0f / dpi;
    float logicalH = (float)(rc.bottom - rc.top) * 96.0f / dpi;

    // White background
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // Bottom border
    pRenderTarget->DrawLine(D2D1::Point2F(0, logicalH - 1),
                            D2D1::Point2F(logicalW, logicalH - 1), pBrushBorder,
                            1.0f);

    // --- RIGHT SIDE ---
    // Logo (red rounded rect) - far right
    float logoW = 48.0f, logoH = 48.0f;
    float logoX = logicalW - logoW - 24.0f; // Padding from right
    float logoY = 8.0f;
    D2D1_RECT_F logoRect =
        D2D1::RectF(logoX, logoY, logoX + logoW, logoY + logoH);

    ID2D1SolidColorBrush *pLogoRed = nullptr;
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xA62626), &pLogoRed);
    if (pLogoRed) {
      pRenderTarget->FillRoundedRectangle(
          D2D1::RoundedRect(logoRect, 8.0f, 8.0f), pLogoRed);
      if (pTextFormatButton) {
        pRenderTarget->DrawText(L"دیوار", 5, pTextFormatButton, logoRect,
                                pBrushWhite);
      }
      pLogoRed->Release();
    }

    // --- LEFT SIDE ---
    // User icon (Circle + semi-circle)
    float userX = 16.0f + 20.0f;
    float userY = 32.0f;
    pRenderTarget->DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(userX, userY - 6), 6.0f, 6.0f), pBrushGray,
        1.5f);
    pRenderTarget->DrawLine(D2D1::Point2F(userX - 8.0f, userY + 8.0f),
                            D2D1::Point2F(userX + 8.0f, userY + 8.0f),
                            pBrushGray, 1.5f);
    pRenderTarget->DrawLine(D2D1::Point2F(userX - 8.0f, userY + 8.0f),
                            D2D1::Point2F(userX - 8.0f, userY + 2.0f),
                            pBrushGray, 1.5f);
    pRenderTarget->DrawLine(D2D1::Point2F(userX + 8.0f, userY + 8.0f),
                            D2D1::Point2F(userX + 8.0f, userY + 2.0f),
                            pBrushGray, 1.5f);

    // "دیوار من" - Use wide rect and LTR reading direction to prevent
    // clipping
    D2D1_RECT_F myDivarRect =
        D2D1::RectF(userX + 16.0f, 16.0f, userX + 150.0f, 48.0f);
    if (pTextFormatBody) {
      pTextFormatBody->SetReadingDirection(
          DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
      pTextFormatBody->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
      pRenderTarget->DrawText(L"دیوار من", 8, pTextFormatBody, myDivarRect,
                              pBrushGray);
      // Restore for other users
      pTextFormatBody->SetReadingDirection(
          DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    }

    if (pRenderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
      DiscardDeviceResources();

    ValidateRect(hWnd, NULL);
    return 0;
  }
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

#include "resource.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
  hInst = hInstance;
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CoInitialize(NULL);
  g_Db.init(); // Initialize Database
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
  DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                      reinterpret_cast<IUnknown **>(&pDWriteFactory));

  // Register main window class
  WNDCLASSEX wcex = {sizeof(WNDCLASSEX)};
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = hInstance;
  wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.lpszClassName = L"DivarScraperPro";
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
  RegisterClassEx(&wcex);

  // Register navbar panel class
  WNDCLASSEX wcNav = {sizeof(WNDCLASSEX)};
  wcNav.lpfnWndProc = NavbarWndProc;
  wcNav.hInstance = hInstance;
  wcNav.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcNav.lpszClassName = L"NavbarPanel";
  wcNav.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  RegisterClassEx(&wcNav);

  // Register custom HeaderBar class for web-like UI
  HeaderBar::RegisterClass(hInstance);
  // Resizable window: WS_OVERLAPPEDWINDOW
  DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  hMainWnd = CreateWindow(L"DivarScraperPro", L"DivarScraper", dwStyle,
                          CW_USEDEFAULT, CW_USEDEFAULT, 1600, 1000, nullptr,
                          nullptr, hInstance, nullptr);
  LayoutEngine::CenterWindow(hMainWnd);
  ShowWindow(hMainWnd, nCmdShow);
  UpdateWindow(hMainWnd);
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}
