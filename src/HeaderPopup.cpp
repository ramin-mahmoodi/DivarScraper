#include "HeaderPopup.h"
#include "DrawHelper.h"
#include <algorithm>
#include <windowsx.h>

extern ID2D1Factory3 *pD2DFactory;
extern IDWriteFactory *pDWriteFactory;

ID2D1Factory3 *HeaderPopup::m_pD2DFactory = nullptr;
IDWriteFactory *HeaderPopup::m_pDWriteFactory = nullptr;

HeaderPopup::HeaderPopup(
    HWND hwnd, const std::vector<MenuItem> &options,
    std::function<void(int, std::wstring, std::string)> onSelect)
    : m_hwnd(hwnd), m_options(options), m_selectCallback(onSelect) {
  m_pD2DFactory = pD2DFactory;
  m_pDWriteFactory = pDWriteFactory;
  m_hSubMenu = nullptr;
  m_hParentPopup = nullptr;
  m_scrollOffset = 0.0f;
  m_maxScroll = 0.0f;
  m_hoveredIndex = -1;
}

HeaderPopup::~HeaderPopup() { DiscardDeviceResources(); }

void HeaderPopup::RegisterWndClass(HINSTANCE hInstance) {
  WNDCLASSEX wcex = {sizeof(WNDCLASSEX)};
  wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = hInstance;
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wcex.lpszClassName = L"HeaderPopup";
  RegisterClassEx(&wcex);
}

HWND HeaderPopup::Show(
    HINSTANCE hInst, HWND parent, int x, int y, int w,
    const std::vector<MenuItem> &options,
    std::function<void(int, std::wstring, std::string)> onSelect,
    HWND hParentPopup, bool alignRight) {

  UINT dpi = GetDpiForWindow(parent);
  if (dpi == 0)
    dpi = 96;
  float scale = dpi / 96.0f;

  int itemH_DIPs = 32;
  int contentH_DIPs = (int)options.size() * itemH_DIPs;
  int maxH_DIPs = 500;

  int clientH_DIPs = (contentH_DIPs > maxH_DIPs) ? maxH_DIPs : contentH_DIPs;

  int clientW_Pix = (int)(w * scale);
  int clientH_Pix = (int)(clientH_DIPs * scale);

  RECT rc = {0, 0, clientW_Pix, clientH_Pix};
  AdjustWindowRectEx(&rc, WS_POPUP | WS_BORDER, FALSE,
                     WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
  int winW = rc.right - rc.left;
  int winH = rc.bottom - rc.top;

  if (alignRight) {
    x -= winW;
  }

  HWND hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"HeaderPopup",
                             L"", WS_POPUP | WS_BORDER, x, y, winW, winH,
                             parent, nullptr, hInst, nullptr);

  if (hwnd) {
    HeaderPopup *pPopup = new HeaderPopup(hwnd, options, onSelect);
    pPopup->m_maxScroll =
        (float)(contentH_DIPs > clientH_DIPs ? contentH_DIPs - clientH_DIPs
                                             : 0);
    pPopup->m_hParentPopup = hParentPopup;

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pPopup);

    ShowWindow(hwnd, SW_SHOWNA);
    UpdateWindow(hwnd);

    if (!hParentPopup) {
      SetCapture(hwnd);
      SetFocus(hwnd);
    }
  }
  return hwnd;
}

LRESULT CALLBACK HeaderPopup::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam) {
  HeaderPopup *pPopup = (HeaderPopup *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (pPopup) {
    return pPopup->HandleMessage(hwnd, msg, wParam, lParam);
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT HeaderPopup::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam) {
  switch (msg) {
  case WM_PAINT:
    OnPaint();
    return 0;
  case WM_SIZE:
    OnSize(LOWORD(lParam), HIWORD(lParam));
    return 0;
  case WM_MOUSEMOVE:
    OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;
  case WM_LBUTTONUP:
    OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;
  case WM_MOUSEWHEEL: {
    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (delta != 0) {
      float step = 32.0f;
      if (delta > 0)
        m_scrollOffset -= step;
      else
        m_scrollOffset += step;
      if (m_scrollOffset < 0)
        m_scrollOffset = 0;
      if (m_scrollOffset > m_maxScroll)
        m_scrollOffset = m_maxScroll;

      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
      OnMouseMove(pt.x, pt.y);

      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_LBUTTONDOWN: {
    RECT rc;
    GetClientRect(hwnd, &rc);
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

    if (!PtInRect(&rc, pt)) {
      if (m_hSubMenu && IsWindow(m_hSubMenu)) {
        RECT rcSub;
        GetWindowRect(m_hSubMenu, &rcSub);
        POINT ptScreen = pt;
        ClientToScreen(hwnd, &ptScreen);
        if (PtInRect(&rcSub, ptScreen)) {
          ReleaseCapture(); // Give up capture so submenu can take it?
          // Actually if we just return, we keep capture.
          // Submenu needs to process its own messages.
          // If parent has capture, parent gets all messages.
          // We should release capture so OS sends message to window under
          // cursor (submenu).
          ReleaseCapture();
          return 0;
        }
      }
      ReleaseCapture();
      HWND parent = m_hParentPopup; // Save before destroy
      DestroyWindow(hwnd);

      while (parent && IsWindow(parent)) {
        HeaderPopup *p = (HeaderPopup *)GetWindowLongPtr(parent, GWLP_USERDATA);
        HWND grandParent = p ? p->m_hParentPopup : nullptr;
        DestroyWindow(parent);
        parent = grandParent;
      }
    }
    return 0;
  }
  case WM_NCDESTROY:
    CloseSubMenu();
    delete this;
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void HeaderPopup::OnSize(int width, int height) {
  if (m_pRenderTarget) {
    m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    UINT dpi = GetDpiForWindow(m_hwnd);
    if (dpi == 0)
      dpi = 96;
    m_pRenderTarget->SetDpi((float)dpi, (float)dpi);
  }
}

HRESULT HeaderPopup::CreateDeviceResources() {
  if (m_pRenderTarget)
    return S_OK;

  RECT rc;
  GetClientRect(m_hwnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

  HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(m_hwnd, size), &m_pRenderTarget);

  if (SUCCEEDED(hr)) {
    UINT dpi = GetDpiForWindow(m_hwnd);
    if (dpi == 0)
      dpi = 96;
    m_pRenderTarget->SetDpi((float)dpi, (float)dpi);

    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                           &m_pWhiteBrush);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.95f, 0.95f),
                                           &m_pHoverBrush);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f),
                                           &m_pTextBrush);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.9f),
                                           &m_pBorderBrush);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.97f, 0.97f, 0.97f),
                                           &m_pTitleBgBrush);

    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"fa-IR",
        &m_pTextFormat);

    if (m_pTextFormat) {
      m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
      m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
      m_pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
  }
  return hr;
}

void HeaderPopup::DiscardDeviceResources() {
  if (m_pRenderTarget) {
    m_pRenderTarget->Release();
    m_pRenderTarget = nullptr;
  }
  if (m_pWhiteBrush) {
    m_pWhiteBrush->Release();
    m_pWhiteBrush = nullptr;
  }
  if (m_pHoverBrush) {
    m_pHoverBrush->Release();
    m_pHoverBrush = nullptr;
  }
  if (m_pTextBrush) {
    m_pTextBrush->Release();
    m_pTextBrush = nullptr;
  }
  if (m_pBorderBrush) {
    m_pBorderBrush->Release();
    m_pBorderBrush = nullptr;
  }
  if (m_pTitleBgBrush) {
    m_pTitleBgBrush->Release();
    m_pTitleBgBrush = nullptr;
  }
  if (m_pTextFormat) {
    m_pTextFormat->Release();
    m_pTextFormat = nullptr;
  }
}

void HeaderPopup::OnPaint() {
  if (FAILED(CreateDeviceResources()))
    return;

  m_pRenderTarget->BeginDraw();
  m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

  UINT dpi = GetDpiForWindow(m_hwnd);
  if (dpi == 0)
    dpi = 96;
  float scale = 96.0f / (float)dpi;

  float itemH = 32.0f;
  RECT rcClient;
  GetClientRect(m_hwnd, &rcClient);
  // w, h in Logical Units (DIPs)
  float w = (float)(rcClient.right - rcClient.left) * scale;
  float h = (float)(rcClient.bottom - rcClient.top) * scale;

  // 1. Set Identity
  m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
  // 2. Clip to Client
  m_pRenderTarget->PushAxisAlignedClip(D2D1::RectF(0, 0, w, h),
                                       D2D1_ANTIALIAS_MODE_ALIASED);

  // 3. Scroll Translate
  D2D1::Matrix3x2F translation =
      D2D1::Matrix3x2F::Translation(0, -m_scrollOffset);
  m_pRenderTarget->SetTransform(translation);

  for (size_t i = 0; i < m_options.size(); ++i) {
    float y1 = i * itemH;
    float y2 = y1 + itemH;

    if (y2 < m_scrollOffset || y1 > m_scrollOffset + h)
      continue;

    D2D1_RECT_F rect = D2D1::RectF(0, y1, w, y2);
    if (m_options[i].isTitle) {
      m_pRenderTarget->FillRectangle(rect, m_pTitleBgBrush);
    } else if ((int)i == m_hoveredIndex) {
      m_pRenderTarget->FillRectangle(rect, m_pHoverBrush);
    }

    D2D1_RECT_F textRect = rect;
    textRect.left += 8;
    textRect.right -= 12;

    // Draw Icon if present (Right Aligned for RTL)
    if (m_options[i].iconType != HeaderIcon::None) {
      float iconSize = 18.0f;
      float iconMargin = 8.0f;
      D2D1_RECT_F iconRect = D2D1::RectF(
          rect.right - iconMargin - iconSize, y1 + (itemH - iconSize) / 2.0f,
          rect.right - iconMargin, y1 + (itemH + iconSize) / 2.0f);

      DrawHelper::DrawIcon(m_pRenderTarget, m_pD2DFactory,
                           m_options[i].iconType, iconRect, m_pTextBrush);

      textRect.right -= (iconSize + iconMargin);
    }

    if (m_options[i].isTitle) {
      IDWriteTextFormat *pTitleFormat = nullptr;
      m_pDWriteFactory->CreateTextFormat(
          L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
          DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"fa-IR",
          &pTitleFormat);

      if (pTitleFormat) {
        pTitleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        pTitleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        pTitleFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        m_pRenderTarget->DrawText(m_options[i].text.c_str(),
                                  (UINT32)m_options[i].text.length(),
                                  pTitleFormat, textRect, m_pTextBrush);
        pTitleFormat->Release();
      }
    } else {
      m_pRenderTarget->DrawText(m_options[i].text.c_str(),
                                (UINT32)m_options[i].text.length(),
                                m_pTextFormat, textRect, m_pTextBrush);
    }

    if (!m_options[i].children.empty()) {
      D2D1_RECT_F arrowRect = D2D1::RectF(4, y1, 20, y2);
      m_pRenderTarget->DrawText(L"‹", 1, m_pTextFormat, arrowRect,
                                m_pTextBrush);
    }

    m_pRenderTarget->DrawLine(D2D1::Point2F(0, rect.bottom),
                              D2D1::Point2F(w, rect.bottom), m_pBorderBrush,
                              0.5f);
  }

  m_pRenderTarget->PopAxisAlignedClip();
  m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

  if (m_maxScroll > 0) {
    float trackH = h;
    float contentH = m_maxScroll + h;
    float thumbH = (h / contentH) * trackH;
    if (thumbH < 20)
      thumbH = 20;
    float thumbY = (m_scrollOffset / m_maxScroll) * (trackH - thumbH);
    D2D1_RECT_F scrollRect = D2D1::RectF(w - 6, thumbY, w - 2, thumbY + thumbH);
    m_pRenderTarget->FillRectangle(scrollRect, m_pBorderBrush);
  }

  m_pRenderTarget->EndDraw();
  ValidateRect(m_hwnd, nullptr);
}

void HeaderPopup::OnMouseMove(int x, int y) {
  POINT scPt = {x, y};
  ClientToScreen(m_hwnd, &scPt);

  if (m_hSubMenu && IsWindow(m_hSubMenu)) {
    RECT rcSub;
    GetWindowRect(m_hSubMenu, &rcSub);
    if (PtInRect(&rcSub, scPt)) {
      ReleaseCapture();
      SetFocus(m_hSubMenu);
      SetCapture(m_hSubMenu);
      return;
    }
  }

  RECT rc;
  GetClientRect(m_hwnd, &rc);

  if (!PtInRect(&rc, {x, y})) {
    if (m_hParentPopup && IsWindow(m_hParentPopup)) {
      RECT rcParent;
      GetWindowRect(m_hParentPopup, &rcParent);
      if (PtInRect(&rcParent, scPt)) {
        ReleaseCapture();
        m_hoveredIndex = -1;
        InvalidateRect(m_hwnd, NULL, FALSE);
        SetCapture(m_hParentPopup);
        return;
      }
    }
  }

  UINT dpi = GetDpiForWindow(m_hwnd);
  float scale = 96.0f / (float)dpi;
  float logicalY = y * scale;
  float scrolledY = logicalY + m_scrollOffset;

  int index = -1;
  float w = (float)(rc.right - rc.left) * scale;
  if (x * scale >= 0 && x * scale <= w && logicalY >= 0 &&
      logicalY <= (rc.bottom - rc.top) * scale) {
    if (scrolledY >= 0) {
      index = (int)(scrolledY / 32.0f);
    }
  }

  if (index < 0 || index >= (int)m_options.size())
    index = -1;

  if (index >= 0 && m_options[index].isTitle)
    index = -1;

  if (index != m_hoveredIndex) {
    m_hoveredIndex = index;
    InvalidateRect(m_hwnd, nullptr, FALSE);

    if (m_hoveredIndex >= 0) {
      ShowSubMenu(m_hoveredIndex);
    } else {
      if (m_hSubMenu && IsWindow(m_hSubMenu)) {
        RECT rcSub;
        GetWindowRect(m_hSubMenu, &rcSub);
        if (PtInRect(&rcSub, scPt)) {
          return;
        }
      }
      CloseSubMenu();
    }
  }
}

void HeaderPopup::ShowSubMenu(int index) {
  if (index < 0 || index >= (int)m_options.size())
    return;

  CloseSubMenu();

  if (m_options[index].children.empty())
    return;

  UINT dpi = GetDpiForWindow(m_hwnd);
  if (dpi == 0)
    dpi = 96;
  float scale = dpi / 96.0f;

  int logicalItemY = (int)((index * 32.0f) - m_scrollOffset);

  // Calculate relative to Client Top-Left (0,0) converted to Screen
  POINT ptItem = {0, (int)(logicalItemY * scale)};
  // Wait, logicalItemY is DIPs. scale converts to Phys.
  // But logicalItemY is relative to Client Top (scrolled? No, physicalItemY
  // should be relative to Window Top). No, logic was: (index * 32) is Y in
  // CONTENT space.
  // - scrollOffset is Y in VIEW space (Client space).
  // So logicalItemY is correct Client Y (DIPs).
  // multiplying by scale gives Client Y (Pixels).

  // Convert Client Y to Screen Y
  POINT ptScreen = {0, (int)(logicalItemY * scale)};
  ClientToScreen(m_hwnd, &ptScreen);
  // ptScreen.y is now correct Screen Y for the item top.

  int subW = 240;
  int subW_Pix = (int)(subW * scale);

  POINT ptClientZero = {0, 0};
  ClientToScreen(m_hwnd, &ptClientZero);

  // Position to the LEFT of the parent menu, overlapping by 2 pixels to close
  // gap
  int subX = ptClientZero.x - subW_Pix + (int)(2 * scale);
  if (subX < 0) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    subX = ptClientZero.x + (rc.right - rc.left) - (int)(2 * scale);
  }

  m_hSubMenu = HeaderPopup::Show(
      (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE), m_hwnd, subX,
      ptScreen.y, subW, m_options[index].children, m_selectCallback, m_hwnd);
}

void HeaderPopup::CloseSubMenu() {
  if (m_hSubMenu && IsWindow(m_hSubMenu)) {
    DestroyWindow(m_hSubMenu);
  }
  m_hSubMenu = nullptr;
}

void HeaderPopup::OnLButtonUp(int x, int y) {
  UINT dpi = GetDpiForWindow(m_hwnd);
  float scale = 96.0f / (float)dpi; // Inverse scale for Phys->Logic
  float logicalY = y * scale;
  float scrolledY = logicalY + m_scrollOffset;

  int index = -1;
  RECT rc;
  GetClientRect(m_hwnd, &rc);
  float scaledW = (float)(rc.right - rc.left) * scale;
  float scaledH = (float)(rc.bottom - rc.top) * scale;

  // Verify click in bounds (logical units)
  if (x * scale >= 0 && x * scale <= scaledW && logicalY >= 0 &&
      logicalY <= scaledH) {
    if (scrolledY >= 0) {
      index = (int)(scrolledY / 32.0f);
      if (index >= 0 && index < (int)m_options.size() &&
          m_options[index].isTitle) {
        index = -1;
      }
    }
  }

  if (index >= 0 && index < (int)m_options.size()) {
    // Allow selection regardless of children (User Request)
    if (m_selectCallback) {
      m_selectCallback(index, m_options[index].text, m_options[index].id);
    }

    ReleaseCapture();
    HWND parent = m_hParentPopup; // Save before destroy
    DestroyWindow(m_hwnd);

    while (parent && IsWindow(parent)) {
      HeaderPopup *p = (HeaderPopup *)GetWindowLongPtr(parent, GWLP_USERDATA);
      HWND grandParent = p ? p->m_hParentPopup : nullptr;
      DestroyWindow(parent);
      parent = grandParent;
    }
  }
}
