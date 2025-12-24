#include "HeaderBar.h"
#include "HeaderPopup.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <windowsx.h>

// Colors (matching React clone)
#define COL_WHITE D2D1::ColorF(1.0f, 1.0f, 1.0f)
#define COL_RED D2D1::ColorF(0.651f, 0.149f, 0.149f)
#define COL_RED_HOVER D2D1::ColorF(0.541f, 0.122f, 0.122f)
#define COL_GRAY D2D1::ColorF(0.44f, 0.44f, 0.44f)
#define COL_BORDER D2D1::ColorF(0.878f, 0.878f, 0.878f)
#define COL_HOVER_BG D2D1::ColorF(0.96f, 0.96f, 0.96f)
#define COL_SHADOW D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.08f)
#define COL_GREEN D2D1::ColorF(0.133f, 0.773f, 0.369f)
#define COL_STOP_RED D2D1::ColorF(0.937f, 0.267f, 0.267f)
#define COL_BLUE D2D1::ColorF(0.231f, 0.510f, 0.965f)

extern HeaderBar *g_HeaderBar; // Defined in main? No, usually main defines, but
                               // here we can define it if needed.
// Actually HeaderBar.cpp usually defines globals if they are used only here or
// shared. In HeaderBar.cpp originally line 17: HeaderBar *g_HeaderBar =
// nullptr; So I keep it.
HeaderBar *g_HeaderBar = nullptr;

extern ID2D1Factory3 *pD2DFactory;
extern IDWriteFactory *pDWriteFactory;

HeaderBar::HeaderBar(HWND hParent) : m_parent(hParent) {
  m_pD2DFactory = pD2DFactory;
  m_pDWriteFactory = pDWriteFactory;

  // Default dimensions, can be updated by OnSize
  RECT rc;
  GetClientRect(hParent, &rc);
  m_width = rc.right; // Initial width
  g_HeaderBar = this; // Set global instance
}

HeaderBar::~HeaderBar() {
  DiscardDeviceResources();
  if (m_hwnd && IsWindow(m_hwnd))
    DestroyWindow(m_hwnd);
  if (g_HeaderBar == this)
    g_HeaderBar = nullptr;
}

bool HeaderBar::RegisterClass(HINSTANCE hInstance) {
  WNDCLASSEXW wc = {sizeof(WNDCLASSEXW)};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = HeaderBar::WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"HeaderBarClass";
  wc.hbrBackground = nullptr;
  return RegisterClassExW(&wc) != 0;
}

HWND HeaderBar::Create(HINSTANCE hInstance) {
  HeaderBar::RegisterClass(hInstance);
  HeaderPopup::RegisterWndClass(hInstance);

  m_hwnd =
      CreateWindowExW(0, L"HeaderBarClass", L"", WS_CHILD | WS_VISIBLE, 0, 0,
                      m_width, m_height, m_parent, nullptr, hInstance, this);

  if (m_hwnd) {
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
  }
  return m_hwnd;
}

void HeaderBar::SetItemText(int index, const std::wstring &text) {
  if (index >= 0 && index < (int)m_items.size()) {
    m_items[index].text = text;
    UpdateLayout(); // Text length might change
    Invalidate();
  }
}

void HeaderBar::SetItemIcon(int itemIndex, HeaderIcon icon) {
  if (itemIndex >= 0 && itemIndex < (int)m_items.size()) {
    m_items[itemIndex].iconType = icon;
    Invalidate();
  }
}

void HeaderBar::SetItemColor(int itemIndex, COLORREF color) {
  if (itemIndex >= 0 && itemIndex < (int)m_items.size()) {
    m_items[itemIndex].textColor = color;
    Invalidate();
  }
}

void HeaderBar::ShowDropdown(
    int itemIndex, const std::vector<MenuItem> &options,
    std::function<void(int, std::wstring, std::string)> onSelect,
    bool alignRight) {
  if (itemIndex < 0 || itemIndex >= (int)m_items.size())
    return;
  auto &item = m_items[itemIndex];

  UINT dpi = GetDpiForWindow(m_hwnd);
  if (dpi == 0)
    dpi = 96;
  float scale = (float)dpi / 96.0f;

  int physX = (int)(item.x * scale);
  int physY = (int)((item.y + item.h) * scale); // Below item

  // Logic: Match button width exactly (User requested compact menu)
  int physW = (int)(item.w * scale);
  int popupW_Pix = physW;

  // RTL Logic: if right aligned, pass RIGHT edge to Popup
  if (item.alignment == HeaderAlignment::Right) {
    physX += physW;
  }

  POINT pt = {physX, physY};
  ClientToScreen(m_hwnd, &pt);

  HeaderPopup::Show(GetModuleHandle(nullptr), m_hwnd, pt.x, pt.y, popupW_Pix,
                    options, onSelect, nullptr,
                    item.alignment == HeaderAlignment::Right);
}

// Create D2D resources
HRESULT HeaderBar::CreateDeviceResources() {
  if (m_pRenderTarget)
    return S_OK;

  RECT rc;
  GetClientRect(m_hwnd, &rc);

  D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

  HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(m_hwnd, size), &m_pRenderTarget);

  if (SUCCEEDED(hr)) {
    // Set DPI for crisp rendering
    UINT dpi = GetDpiForWindow(m_hwnd);
    if (dpi == 0)
      dpi = 96;
    m_pRenderTarget->SetDpi((float)dpi, (float)dpi);

    m_pRenderTarget->CreateSolidColorBrush(COL_WHITE, &m_pWhiteBrush);
    m_pRenderTarget->CreateSolidColorBrush(COL_BORDER, &m_pBorderBrush);
    m_pRenderTarget->CreateSolidColorBrush(COL_RED, &m_pRedBrush);
    m_pRenderTarget->CreateSolidColorBrush(COL_GRAY, &m_pGrayBrush);
    m_pRenderTarget->CreateSolidColorBrush(COL_HOVER_BG, &m_pHoverBrush);
    m_pRenderTarget->CreateSolidColorBrush(COL_GRAY, &m_pTextBrush);

    // Create text formats
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"fa-IR",
        &m_pTextFormat);

    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"fa-IR", &m_pLogoFormat);

    if (m_pTextFormat) {
      m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
      m_pTextFormat->SetReadingDirection(
          DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    }
    if (m_pLogoFormat) {
      m_pLogoFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      m_pLogoFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
  }

  return hr;
}

void HeaderBar::DiscardDeviceResources() {
  if (m_pWhiteBrush) {
    m_pWhiteBrush->Release();
    m_pWhiteBrush = nullptr;
  }
  if (m_pBorderBrush) {
    m_pBorderBrush->Release();
    m_pBorderBrush = nullptr;
  }
  if (m_pRedBrush) {
    m_pRedBrush->Release();
    m_pRedBrush = nullptr;
  }
  if (m_pGrayBrush) {
    m_pGrayBrush->Release();
    m_pGrayBrush = nullptr;
  }
  if (m_pHoverBrush) {
    m_pHoverBrush->Release();
    m_pHoverBrush = nullptr;
  }
  if (m_pTextBrush) {
    m_pTextBrush->Release();
    m_pTextBrush = nullptr;
  }
  if (m_pTextFormat) {
    m_pTextFormat->Release();
    m_pTextFormat = nullptr;
  }
  if (m_pLogoFormat) {
    m_pLogoFormat->Release();
    m_pLogoFormat = nullptr;
  }
  if (m_pRenderTarget) {
    m_pRenderTarget->Release();
    m_pRenderTarget = nullptr;
  }
}

// HeaderBar.cpp implementation updates

// Add Divider handling in UpdateLayout
void HeaderBar::UpdateLayout() {
  if (m_width <= 0)
    return;

  UINT dpi = GetDpiForWindow(m_hwnd);
  if (dpi == 0)
    dpi = 96;
  float scale = 96.0f / (float)dpi;
  float logicalW = (float)m_width * scale;
  float logicalH = (float)m_height * scale;

  float rightX = logicalW - m_padding;
  float leftX = (float)m_padding;

  // Set sizes first
  for (auto &item : m_items) {
    if (item.type == HeaderItemType::Logo) {
      if (item.w <= 0)
        item.w = 56;
      if (item.h <= 0)
        item.h = 48;
    } else if (item.type == HeaderItemType::Dropdown) {
      // User request: Smaller width (was 140)
      if (item.w <= 0)
        item.w = 110;
      if (item.h <= 0)
        item.h = 32;
    } else if (item.type == HeaderItemType::Button) {
      if (item.w <= 0)
        item.w = 70;
      if (item.h <= 0)
        item.h = 32;
    } else if (item.type == HeaderItemType::UserArea) {
      if (item.w <= 0)
        item.w = 120;
      if (item.h <= 0)
        item.h = 48;
    } else if (item.type == HeaderItemType::Divider) {
      if (item.w <= 0)
        item.w = 1;
      if (item.h <= 0)
        item.h = 24;
    }
    item.y = (logicalH - item.h) / 2.0f;
  }

  // Pass 1: Right Aligned Items
  for (auto &item : m_items) {
    if (item.alignment == HeaderAlignment::Right) {
      item.x = rightX - item.w;
      rightX = item.x - m_gap;
    }
  }

  // Pass 2: Left Aligned Items
  for (auto &item : m_items) {
    if (item.alignment == HeaderAlignment::Left) {
      item.x = leftX;
      leftX = item.x + item.w + m_gap;
    }
  }
}

// Window procedure
LRESULT CALLBACK HeaderBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  if (g_HeaderBar) {
    if (!g_HeaderBar->m_hwnd) {
      g_HeaderBar->m_hwnd = hwnd;
    }
    return g_HeaderBar->HandleMessage(hwnd, msg, wParam, lParam);
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT HeaderBar::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam,
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

  case WM_MOUSELEAVE:
    OnMouseLeave();
    return 0;

  case WM_LBUTTONDOWN:
    OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;

  case WM_LBUTTONUP:
    OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;

  case WM_SETCURSOR: {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    int hit = HitTest(pt.x, pt.y);
    if (hit >= 0 && m_items[hit].type != HeaderItemType::Spacer &&
        m_items[hit].type != HeaderItemType::Divider) {
      SetCursor(LoadCursor(nullptr, IDC_HAND));
      return TRUE;
    }
    break;
  }

  case WM_ERASEBKGND:
    return 1;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int HeaderBar::HitTest(int x, int y) {
  UINT dpi = GetDpiForWindow(m_hwnd);
  if (dpi == 0)
    dpi = 96;
  float scale = 96.0f / (float)dpi;
  float logicalX = (float)x * scale;
  float logicalY = (float)y * scale;

  for (size_t i = 0; i < m_items.size(); i++) {
    auto &item = m_items[i];
    if (logicalX >= item.x && logicalX <= item.x + item.w &&
        logicalY >= item.y && logicalY <= item.y + item.h) {
      return (int)i;
    }
  }
  return -1;
}

void HeaderBar::OnMouseMove(int x, int y) {
  int hit = HitTest(x, y);
  if (hit != m_hoveredItem) {
    if (m_hoveredItem >= 0)
      m_items[m_hoveredItem].isHovered = false;
    if (hit >= 0)
      m_items[hit].isHovered = true;
    m_hoveredItem = hit;
    Invalidate();
  }
  TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, m_hwnd, 0};
  TrackMouseEvent(&tme);
}

void HeaderBar::OnMouseLeave() {
  if (m_hoveredItem >= 0) {
    m_items[m_hoveredItem].isHovered = false;
    m_hoveredItem = -1;
    Invalidate();
  }
}

void HeaderBar::OnLButtonDown(int x, int y) {
  int hit = HitTest(x, y);
  if (hit >= 0) {
    m_items[hit].isPressed = true;
    m_pressedItem = hit;
    Invalidate();
  }
}

void HeaderBar::OnLButtonUp(int x, int y) {
  if (m_pressedItem >= 0) {
    auto &item = m_items[m_pressedItem];
    item.isPressed = false;

    int hit = HitTest(x, y);
    if (hit == m_pressedItem && item.onClick) {
      item.onClick();
    }

    m_pressedItem = -1;
    Invalidate();
  }
}

void HeaderBar::OnSize(int width, int height) {
  m_width = width;
  m_height = height;
  if (m_pRenderTarget) {
    m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    UINT dpi = GetDpiForWindow(m_hwnd);
    if (dpi == 0)
      dpi = 96;
    m_pRenderTarget->SetDpi((float)dpi, (float)dpi);
  }
  UpdateLayout();
  Invalidate();
}

void HeaderBar::Invalidate() { InvalidateRect(m_hwnd, nullptr, FALSE); }

// Update OnPaint to handle Divider
void HeaderBar::OnPaint() {
  HRESULT hr = CreateDeviceResources();
  if (FAILED(hr))
    return;

  m_pRenderTarget->BeginDraw();
  m_pRenderTarget->Clear(COL_WHITE);
  DrawShadow();

  m_pRenderTarget->DrawLine(
      D2D1::Point2F(0, (float)m_height - 0.5f),
      D2D1::Point2F((float)m_width, (float)m_height - 0.5f), m_pBorderBrush,
      1.0f);

  for (auto &item : m_items) {
    switch (item.type) {
    case HeaderItemType::Logo:
      DrawLogo(item);
      break;
    case HeaderItemType::Dropdown:
      DrawDropdown(item);
      break;
    case HeaderItemType::Button:
      DrawButton(item);
      break;
    case HeaderItemType::UserArea:
      DrawUserArea(item);
      break;
    case HeaderItemType::Divider:
      DrawDivider(item);
      break;
    case HeaderItemType::Spacer:
      break;
    }
  }

  hr = m_pRenderTarget->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    DiscardDeviceResources();
  }
  ValidateRect(m_hwnd, nullptr);
}

void HeaderBar::DrawShadow() {
  ID2D1GradientStopCollection *pGradientStops = nullptr;
  D2D1_GRADIENT_STOP stops[2];
  stops[0].color = D2D1::ColorF(0, 0, 0, 0.05f);
  stops[0].position = 0.0f;
  stops[1].color = D2D1::ColorF(0, 0, 0, 0.0f);
  stops[1].position = 1.0f;

  m_pRenderTarget->CreateGradientStopCollection(stops, 2, &pGradientStops);

  if (pGradientStops) {
    ID2D1LinearGradientBrush *pShadowBrush = nullptr;
    m_pRenderTarget->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(0, (float)m_height),
            D2D1::Point2F(0, (float)m_height + 4)),
        pGradientStops, &pShadowBrush);

    if (pShadowBrush) {
      m_pRenderTarget->FillRectangle(
          D2D1::RectF(0, (float)m_height, (float)m_width, (float)m_height + 4),
          pShadowBrush);
      pShadowBrush->Release();
    }
    pGradientStops->Release();
  }
}

// Basic SVG Path Parser Helper
static void ParseSVGPath(ID2D1GeometrySink *pSink, const char *path) {
  const char *p = path;
  D2D1_POINT_2F cur = {0, 0};
  D2D1_POINT_2F lastCtrl = {0, 0};

  auto SkipSeparators = [&]() {
    while (*p &&
           (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r'))
      p++;
  };

  auto ReadFloat = [&]() -> float {
    SkipSeparators();
    char *end;
    float f = strtof(p, &end);
    p = end;
    return f;
  };

  char cmd = 0;
  while (*p) {
    SkipSeparators();
    if (!*p)
      break;
    if (isalpha(*p)) {
      cmd = *p;
      p++;
    }
    // implicit repeat uses last cmd

    switch (cmd) {
    case 'M': {
      cur.x = ReadFloat();
      cur.y = ReadFloat();
      pSink->BeginFigure(cur, D2D1_FIGURE_BEGIN_FILLED);
      lastCtrl = cur;
      break;
    }
    case 'm': {
      cur.x += ReadFloat();
      cur.y += ReadFloat();
      pSink->BeginFigure(cur, D2D1_FIGURE_BEGIN_FILLED);
      lastCtrl = cur;
      break;
    }
    case 'L': {
      cur.x = ReadFloat();
      cur.y = ReadFloat();
      pSink->AddLine(cur);
      lastCtrl = cur;
      break;
    }
    case 'l': {
      cur.x += ReadFloat();
      cur.y += ReadFloat();
      pSink->AddLine(cur);
      lastCtrl = cur;
      break;
    }
    case 'H': {
      cur.x = ReadFloat();
      pSink->AddLine(cur);
      lastCtrl = cur;
      break;
    }
    case 'h': {
      cur.x += ReadFloat();
      pSink->AddLine(cur);
      lastCtrl = cur;
      break;
    }
    case 'V': {
      cur.y = ReadFloat();
      pSink->AddLine(cur);
      lastCtrl = cur;
      break;
    }
    case 'v': {
      cur.y += ReadFloat();
      pSink->AddLine(cur);
      lastCtrl = cur;
      break;
    }
    case 'C': {
      D2D1_POINT_2F p1 = {ReadFloat(), ReadFloat()};
      D2D1_POINT_2F p2 = {ReadFloat(), ReadFloat()};
      cur = {ReadFloat(), ReadFloat()};
      pSink->AddBezier(D2D1::BezierSegment(p1, p2, cur));
      lastCtrl = p2;
      break;
    }
    case 'c': {
      D2D1_POINT_2F p1 = {cur.x + ReadFloat(), cur.y + ReadFloat()};
      D2D1_POINT_2F p2 = {cur.x + ReadFloat(), cur.y + ReadFloat()};
      cur = {cur.x + ReadFloat(), cur.y + ReadFloat()};
      pSink->AddBezier(D2D1::BezierSegment(p1, p2, cur));
      lastCtrl = p2;
      break;
    }
    case 'S': {
      // Reflect lastCtrl over cur
      D2D1_POINT_2F p1 = {2 * cur.x - lastCtrl.x, 2 * cur.y - lastCtrl.y};
      D2D1_POINT_2F p2 = {ReadFloat(), ReadFloat()};
      cur = {ReadFloat(), ReadFloat()};
      pSink->AddBezier(D2D1::BezierSegment(p1, p2, cur));
      lastCtrl = p2;
      break;
    }
    case 's': {
      // Reflect lastCtrl over cur
      D2D1_POINT_2F p1 = {2 * cur.x - lastCtrl.x, 2 * cur.y - lastCtrl.y};
      D2D1_POINT_2F p2 = {cur.x + ReadFloat(), cur.y + ReadFloat()};
      cur = {cur.x + ReadFloat(), cur.y + ReadFloat()};
      pSink->AddBezier(D2D1::BezierSegment(p1, p2, cur));
      lastCtrl = p2;
      break;
    }
    case 'Z':
    case 'z': {
      pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
      lastCtrl = cur;
      break;
    }
    case 'a': {
      float rx = ReadFloat();
      float ry = ReadFloat();
      float rot = ReadFloat();
      float large = ReadFloat();
      float sweep = ReadFloat();
      float dx = ReadFloat();
      float dy = ReadFloat();
      D2D1_POINT_2F end = {cur.x + dx, cur.y + dy};
      pSink->AddArc(D2D1::ArcSegment(
          end, D2D1::SizeF(rx, ry), rot,
          sweep > 0.5f ? D2D1_SWEEP_DIRECTION_CLOCKWISE
                       : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
          large > 0.5f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
      cur = end;
      lastCtrl = cur;
      break;
    }
    // Add more if needed (Q, T, A absolute)
    default:
      ReadFloat();
      // Skip logic might need to be smarter if default consumes only one float?
      // For safety, assume loop continues.
      break;
    }
  }
}

void HeaderBar::DrawLogo(const HeaderItem &item) {
  ID2D1PathGeometry *pPath = nullptr;
  m_pD2DFactory->CreatePathGeometry(&pPath);
  if (pPath) {
    ID2D1GeometrySink *pSink = nullptr;
    pPath->Open(&pSink);
    pSink->SetFillMode(D2D1_FILL_MODE_WINDING);

    // Logo Path from Logo.svg (Divar)
    const char *d =
        "M8.386 14.617H8.28a.712.712 0 0 "
        "1-.595-.806c.473-3.117.63-8.092.63-8.127.035-.386.333-.7.736-.683a."
        "715.715 0 0 1 .683.718c0 .21-.175 5.097-.648 8.303a.7.7 0 0 "
        "1-.7.595Zm10.037 1.296a.693.693 0 0 1-.666-.49.688.688 0 0 1 "
        ".455-.876c3.31-1.05 3.363-1.857 "
        "3.381-2.295.035-.683-.49-1.558-.7-1.856a.701.701 0 1 1 "
        "1.138-.824c.105.14 1.05 1.454.963 2.768-.087 1.594-1.314 2.575-4.361 "
        "3.556a.845.845 0 0 0-.105.009c-.035.004-.07.009-.105.009Zm-9.512 "
        "2.47a.656.656 0 0 1-.543-.262.683.683 0 0 1 .123-.981c1.436-1.139 "
        "2.4-2.155 "
        "3.03-3.048-.35-.175-.718-.438-.91-.876-.176-.403-.281-1.069.332-1.962."
        "876-1.296 1.752-1.594 2.084-1.664a.871.871 0 0 1 "
        "1.016.613c.088.315.298 1.314-.332 2.838.84-.018 1.506-.245 "
        "2.014-.666.98-.788 1.033-2.12 1.033-2.137a.715.715 0 0 1 "
        ".719-.683.715.715 0 0 1 .683.718c0 .07-.07 1.944-1.524 "
        "3.17-.911.771-2.155 1.104-3.661.982-.718 1.103-1.857 2.4-3.626 "
        "3.8a.708.708 0 0 1-.438.158Zm3.801-7.076a3.134 3.134 0 0 "
        "0-.63.735c-.228.35-.246.543-.228.596.035.088.21.175.385.245.333-.665."
        "438-1.191.473-1.576Zm.175 4.922a.71.71 0 0 0 .7.683h.036c.053 0 "
        "1.226-.035 2.908-.42a.702.702 0 0 0 .543-.841.702.702 0 0 "
        "0-.841-.543c-1.56.332-2.663.385-2.68.385a.704.704 0 0 "
        "0-.666.736ZM1.134 18.103c.122.175.35.28.56.28.14 0 .28-.035.42-.122 "
        "4.45-3.24 4.625-7.48 4.625-7.655 "
        "0-.385-.298-.7-.683-.718-.386-.018-.7.298-.718.683 0 .14-.176 "
        "3.731-4.047 6.551a.701.701 0 0 0-.157.981Z";

    ParseSVGPath(pSink, d);

    pSink->Close();
    pSink->Release();

    // Scale logo to fit (Original 24x24)
    float scale = (std::min)(item.w / 24.0f, item.h / 24.0f);
    float tx = item.x + (item.w - 24.0f * scale) / 2.0f;
    float ty = item.y + (item.h - 24.0f * scale) / 2.0f;

    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                                  D2D1::Matrix3x2F::Translation(tx, ty));
    m_pRenderTarget->FillGeometry(pPath, m_pRedBrush); // Red Divar Color
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    pPath->Release();
  }
}

void HeaderBar::DrawDivider(const HeaderItem &item) {
  m_pRenderTarget->DrawLine(D2D1::Point2F(item.x, item.y),
                            D2D1::Point2F(item.x, item.y + item.h),
                            m_pBorderBrush, 1.0f);
}

void HeaderBar::DrawDropdown(const HeaderItem &item) {
  D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
      D2D1::RectF(item.x, item.y, item.x + item.w, item.y + item.h), 4.0f,
      4.0f);

  if (item.isHovered || item.isPressed) {
    m_pRenderTarget->FillRoundedRectangle(rr, m_pHoverBrush);
  }

  // Draw Icon (if exists, e.g. MapPin)
  float textX = item.x + 8;
  if (item.iconType != HeaderIcon::None) {
    D2D1_RECT_F iconRect =
        D2D1::RectF(item.x + 8, item.y + 6, item.x + 8 + 20, item.y + 6 + 20);
    DrawIcon(item.iconType, iconRect, m_pGrayBrush);
    textX += 24;
  }

  // Text
  D2D1_RECT_F textRect =
      D2D1::RectF(textX, item.y, item.x + item.w - 20, item.y + item.h);
  std::wstring displayText = item.text;
  if (!item.dropdownItems.empty() && item.selectedIndex >= 0 &&
      item.selectedIndex < (int)item.dropdownItems.size()) {
    displayText = item.dropdownItems[item.selectedIndex];
  }
  m_pRenderTarget->DrawText(displayText.c_str(), (UINT32)displayText.length(),
                            m_pTextFormat, textRect, m_pGrayBrush);

  // Chevron Down
  D2D1_RECT_F arrowRect = D2D1::RectF(item.x + item.w - 24, item.y + 6,
                                      item.x + item.w - 4, item.y + 6 + 20);
  DrawIcon(HeaderIcon::ChevronDown, arrowRect, m_pGrayBrush);
}

void HeaderBar::DrawButton(const HeaderItem &item) {
  D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
      D2D1::RectF(item.x, item.y, item.x + item.w, item.y + item.h), 4.0f,
      4.0f);

  if (item.isHovered || item.isPressed) {
    m_pRenderTarget->FillRoundedRectangle(rr, m_pHoverBrush);
  }

  ID2D1SolidColorBrush *textBrush = m_pGrayBrush;
  if (item.text == L"شروع") {
    m_pRenderTarget->CreateSolidColorBrush(COL_GREEN, &textBrush);
  } else if (item.text == L"توقف") {
    m_pRenderTarget->CreateSolidColorBrush(COL_STOP_RED, &textBrush);
  } else if (item.text == L"خروجی") {
    m_pRenderTarget->CreateSolidColorBrush(COL_BLUE, &textBrush);
  }

  // Draw Icon
  float textX = item.x;
  if (item.iconType != HeaderIcon::None) {
    D2D1_RECT_F iconRect =
        D2D1::RectF(item.x + 8, item.y + 8, item.x + 8 + 16, item.y + 8 + 16);
    DrawIcon(item.iconType, iconRect, textBrush);
    textX += 24;
  }

  D2D1_RECT_F textRect =
      D2D1::RectF(textX, item.y, item.x + item.w, item.y + item.h);
  m_pRenderTarget->DrawText(item.text.c_str(), (UINT32)item.text.length(),
                            m_pTextFormat, textRect, textBrush);

  if (textBrush != m_pGrayBrush) {
    textBrush->Release();
  }
}

void HeaderBar::DrawUserArea(const HeaderItem &item) {
  if (item.isHovered || item.isPressed) {
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
        D2D1::RectF(item.x, item.y, item.x + item.w, item.y + item.h), 4.0f,
        4.0f);
    m_pRenderTarget->FillRoundedRectangle(rr, m_pHoverBrush);
  }

  // Text "دیوار من" (Red) - Positioned on Left/Center
  // Icon (Red) - Positioned on Right

  // Assuming item.w is enough (e.g. 100px)
  // Icon on Right:
  D2D1_RECT_F iconRect = D2D1::RectF(item.x + item.w - 32, item.y + 12,
                                     item.x + item.w - 8, item.y + 12 + 24);
  DrawIcon(HeaderIcon::User, iconRect, m_pRedBrush); // Red Icon

  // Text to the left of Icon
  D2D1_RECT_F textRect =
      D2D1::RectF(item.x, item.y, item.x + item.w - 36, item.y + item.h);
  m_pRenderTarget->DrawText(L"دیوار من", 8, m_pTextFormat, textRect,
                            m_pRedBrush); // Red Text
}

void HeaderBar::DrawIcon(HeaderIcon icon, D2D1_RECT_F rect, ID2D1Brush *brush) {
  ID2D1PathGeometry *pPath = nullptr;
  m_pD2DFactory->CreatePathGeometry(&pPath);
  if (!pPath)
    return;

  ID2D1GeometrySink *pSink = nullptr;
  pPath->Open(&pSink);
  pSink->SetFillMode(D2D1_FILL_MODE_WINDING);

  // Standard Material/Heroicon Paths (24x24 viewbox)
  const char *d = "";
  if (icon == HeaderIcon::MapPin) {
    d = "M12 2C8.13 2 5 5.13 5 9c0 5.25 7 13 7 13s7-7.75 "
        "7-13c0-3.87-3.13-7-7-7zm0 9.5c-1.38 0-2.5-1.12-2.5-2.5s1.12-2.5 "
        "2.5-2.5 2.5 1.12 2.5 2.5-1.12 2.5-2.5 2.5z";
  } else if (icon == HeaderIcon::ChevronDown) {
    d = "M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z";
  } else if (icon == HeaderIcon::Play) {
    d = "M8 5v14l11-7z";
  } else if (icon == HeaderIcon::Stop) {
    d = "M6 6h12v12H6z";
  } else if (icon == HeaderIcon::Download) {
    d = "M19 9h-4V3H9v6H5l7 7 7-7zM5 18v2h14v-2H5z";
  } else if (icon == HeaderIcon::User) {
    // Lucide User Icon (Stroke-based)
    // Head Circle: cx=12, cy=7, r=4
    pSink->BeginFigure(D2D1::Point2F(16, 7), D2D1_FIGURE_BEGIN_HOLLOW);
    pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(8, 7), D2D1::SizeF(4, 4), 0,
                                   D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                   D2D1_ARC_SIZE_LARGE));
    pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(16, 7), D2D1::SizeF(4, 4), 0,
                                   D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                   D2D1_ARC_SIZE_LARGE));
    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);

    // Body Path: M19 21v-2a4 4 0 0 0-4-4H9a4 4 0 0 0-4 4v2
    // Start at (19, 21), line to (19, 19), arc to (15, 15), line to (9, 15),
    // arc to (5, 19), line to (5, 21)
    pSink->BeginFigure(D2D1::Point2F(19, 21), D2D1_FIGURE_BEGIN_HOLLOW);
    pSink->AddLine(D2D1::Point2F(19, 19));
    pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(15, 15), D2D1::SizeF(4, 4), 0,
                                   D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                   D2D1_ARC_SIZE_SMALL));
    pSink->AddLine(D2D1::Point2F(9, 15));
    pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(5, 19), D2D1::SizeF(4, 4), 0,
                                   D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                   D2D1_ARC_SIZE_SMALL));
    pSink->AddLine(D2D1::Point2F(5, 21));
    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
  }

  if (icon != HeaderIcon::User) {
    ParseSVGPath(pSink, d);
  }

  pSink->Close();
  pSink->Release();

  // Scale and center in rect
  // Assuming paths are 24x24
  float w = rect.right - rect.left;
  float h = rect.bottom - rect.top;
  float scale = (std::min)(w / 24.0f, h / 24.0f);
  float tx = rect.left + (w - 24.0f * scale) / 2.0f;
  float ty = rect.top + (h - 24.0f * scale) / 2.0f;

  m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                                D2D1::Matrix3x2F::Translation(tx, ty));
  if (icon == HeaderIcon::User) {
    // Stroke-based rendering for User icon
    m_pRenderTarget->DrawGeometry(pPath, brush, 2.0f);
  } else {
    m_pRenderTarget->FillGeometry(pPath, brush);
  }
  m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

  pPath->Release();
}

// Item management
void HeaderBar::AddItem(const HeaderItem &item) {
  m_items.push_back(item);
  UpdateLayout();
}

void HeaderBar::ClearItems() {
  m_items.clear();
  UpdateLayout();
}

void HeaderBar::SetItemEnabled(int index, bool enabled) {
  if (index >= 0 && index < (int)m_items.size()) {
    m_items[index].isEnabled = enabled;
    Invalidate();
  }
}

void HeaderBar::SetDropdownItems(int itemIndex,
                                 const std::vector<std::wstring> &items) {
  if (itemIndex >= 0 && itemIndex < (int)m_items.size()) {
    m_items[itemIndex].dropdownItems = items;
    Invalidate();
  }
}

void HeaderBar::SetDropdownSelection(int itemIndex, int selection) {
  if (itemIndex >= 0 && itemIndex < (int)m_items.size()) {
    m_items[itemIndex].selectedIndex = selection;
    Invalidate();
  }
}

int HeaderBar::GetDropdownSelection(int itemIndex) const {
  if (itemIndex >= 0 && itemIndex < (int)m_items.size()) {
    return m_items[itemIndex].selectedIndex;
  }
  return -1;
}
