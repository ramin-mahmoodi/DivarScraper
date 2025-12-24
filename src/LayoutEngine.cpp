#include "LayoutEngine.h"

LayoutEngine::LayoutEngine() : m_hParent(NULL), m_currentDpi(96) {}

LayoutEngine::~LayoutEngine() {}

void LayoutEngine::Initialize(HWND hParent) {
  m_hParent = hParent;
  m_currentDpi = GetDpiForWindow(hParent);
}

void LayoutEngine::Reset() { m_controls.clear(); }

void LayoutEngine::RegisterAnchor(HWND hWnd, bool l, bool t, bool r, bool b,
                                  Margin m, SIZE minSz) {
  ControlLayout cl;
  cl.hWnd = hWnd;
  cl.type = LayoutType::Anchor;
  cl.anchors = {l, t, r, b};
  cl.margin = m;
  cl.minSize = minSz;
  cl.dockSide = DockSide::None;
  cl.fixedSize = 0;

  // Snapshot current position relative to parent
  RECT rc;
  GetWindowRect(hWnd, &rc);
  POINT pt = {rc.left, rc.top};
  ScreenToClient(m_hParent, &pt);
  cl.originalRect.left = pt.x;
  cl.originalRect.top = pt.y;
  cl.originalRect.right = pt.x + (rc.right - rc.left);
  cl.originalRect.bottom = pt.y + (rc.bottom - rc.top);

  RECT rcParent;
  GetClientRect(m_hParent, &rcParent);
  cl.originalParentSize.cx = rcParent.right;
  cl.originalParentSize.cy = rcParent.bottom;

  m_controls.push_back(cl);
}

void LayoutEngine::RegisterDock(HWND hWnd, DockSide side, int size, Margin m,
                                SIZE minSz) {
  ControlLayout cl;
  cl.hWnd = hWnd;
  cl.type = LayoutType::Dock;
  cl.dockSide = side;
  cl.fixedSize = size;
  cl.margin = m;
  cl.minSize = minSz;
  // Anchors not used for dock
  cl.anchors = {false, false, false, false};

  m_controls.push_back(cl);
}

void LayoutEngine::Apply(int clientW, int clientH) {
  if (clientW <= 0 || clientH <= 0 || !m_hParent || m_controls.empty())
    return;

  HDWP hdwp = BeginDeferWindowPos((int)m_controls.size());
  if (!hdwp)
    return;

  RECT remaining = {0, 0, clientW, clientH};

  for (const auto &ctrl : m_controls) {
    if (!IsWindow(ctrl.hWnd))
      continue;

    int x, y, w, h;
    bool update = false;

    if (ctrl.type == LayoutType::Dock) {
      update = true;
      // Apply Margins
      int ml = ctrl.margin.left;
      int mt = ctrl.margin.top;
      int mr = ctrl.margin.right;
      int mb = ctrl.margin.bottom;

      switch (ctrl.dockSide) {
      case DockSide::Top:
        x = remaining.left + ml;
        y = remaining.top + mt;
        w = (remaining.right - remaining.left) - (ml + mr);
        h = ctrl.fixedSize; // Height is fixed
        remaining.top += h + mt + mb;
        break;
      case DockSide::Bottom:
        h = ctrl.fixedSize;
        x = remaining.left + ml;
        y = remaining.bottom - mb - h;
        w = (remaining.right - remaining.left) - (ml + mr);
        remaining.bottom -= (h + mt + mb);
        break;
      case DockSide::Left:
        x = remaining.left + ml;
        y = remaining.top + mt;
        w = ctrl.fixedSize;
        h = (remaining.bottom - remaining.top) - (mt + mb);
        remaining.left += w + ml + mr;
        break;
      case DockSide::Right:
        w = ctrl.fixedSize;
        x = remaining.right - mr - w;
        y = remaining.top + mt;
        h = (remaining.bottom - remaining.top) - (mt + mb);
        remaining.right -= (w + ml + mr);
        break;
      case DockSide::Fill:
        x = remaining.left + ml;
        y = remaining.top + mt;
        w = (remaining.right - remaining.left) - (ml + mr);
        h = (remaining.bottom - remaining.top) - (mt + mb);
        break;
      default:
        update = false;
        break;
      }
    } else if (ctrl.type == LayoutType::Anchor) {
      update = true;
      int origW = ctrl.originalRect.right - ctrl.originalRect.left;
      int origH = ctrl.originalRect.bottom - ctrl.originalRect.top;

      // Horizontal
      if (ctrl.anchors.left && ctrl.anchors.right) {
        // Stretch
        int distL = ctrl.originalRect.left;
        int distR = ctrl.originalParentSize.cx - ctrl.originalRect.right;
        x = distL;
        w = clientW - distL - distR;
      } else if (ctrl.anchors.right) {
        // Move with right
        int distR = ctrl.originalParentSize.cx - ctrl.originalRect.right;
        x = clientW - distR - origW;
        w = origW;
      } else {
        // Left or None (Left default)
        x = ctrl.originalRect.left;
        w = origW;
      }

      // Vertical
      if (ctrl.anchors.top && ctrl.anchors.bottom) {
        // Stretch
        int distT = ctrl.originalRect.top;
        int distB = ctrl.originalParentSize.cy - ctrl.originalRect.bottom;
        y = distT;
        h = clientH - distT - distB;
      } else if (ctrl.anchors.bottom) {
        // Move with bottom
        int distB = ctrl.originalParentSize.cy - ctrl.originalRect.bottom;
        y = clientH - distB - origH;
        h = origH;
      } else {
        // Top or None
        y = ctrl.originalRect.top;
        h = origH;
      }
    }

    if (update) {
      // Apply MinSize
      if (w < ctrl.minSize.cx)
        w = ctrl.minSize.cx;
      if (h < ctrl.minSize.cy)
        h = ctrl.minSize.cy;

      hdwp = DeferWindowPos(hdwp, ctrl.hWnd, NULL, x, y, w, h,
                            SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }

  EndDeferWindowPos(hdwp);
}

void LayoutEngine::UpdateDPI(int newDpi, int oldDpi) {
  if (oldDpi == 0)
    oldDpi = 96;
  float scale = (float)newDpi / (float)oldDpi;
  m_currentDpi = newDpi;

  for (auto &ctrl : m_controls) {
    // Rescale Margins
    ctrl.margin.left = (int)(ctrl.margin.left * scale);
    ctrl.margin.top = (int)(ctrl.margin.top * scale);
    ctrl.margin.right = (int)(ctrl.margin.right * scale);
    ctrl.margin.bottom = (int)(ctrl.margin.bottom * scale);

    // Rescale MinSize
    ctrl.minSize.cx = (int)(ctrl.minSize.cx * scale);
    ctrl.minSize.cy = (int)(ctrl.minSize.cy * scale);

    if (ctrl.type == LayoutType::Dock) {
      ctrl.fixedSize = (int)(ctrl.fixedSize * scale);
    } else if (ctrl.type == LayoutType::Anchor) {
      // Rescale Original Rect & Parent Size to simulate "as if it was created
      // at new DPI" This ensures future anchors logic works on the new
      // coordinate system
      ctrl.originalRect.left = (int)(ctrl.originalRect.left * scale);
      ctrl.originalRect.top = (int)(ctrl.originalRect.top * scale);
      ctrl.originalRect.right = (int)(ctrl.originalRect.right * scale);
      ctrl.originalRect.bottom = (int)(ctrl.originalRect.bottom * scale);

      ctrl.originalParentSize.cx = (int)(ctrl.originalParentSize.cx * scale);
      ctrl.originalParentSize.cy = (int)(ctrl.originalParentSize.cy * scale);
    }
  }
}

SIZE LayoutEngine::GetMinTrackSize() {
  // Return minimum window size for proper UI
  return {1200, 800};
}

void LayoutEngine::CenterWindow(HWND hWnd) {
  if (!hWnd)
    return;

  RECT rc;
  GetWindowRect(hWnd, &rc);
  int winW = rc.right - rc.left;
  int winH = rc.bottom - rc.top;

  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int screenH = GetSystemMetrics(SM_CYSCREEN);

  int x = (screenW - winW) / 2;
  int y = (screenH - winH) / 2;

  SetWindowPos(hWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// --- D2D Text Input Implementation ---

void LayoutEngine::CreateD2DInput(RECT bounds, float letterSpacing,
                                  float fontSize, LPCWSTR placeholder,
                                  int maxLen, bool numbersOnly) {
  m_d2dInput.bounds = bounds;
  m_d2dInput.letterSpacing = letterSpacing;
  m_d2dInput.fontSize = fontSize;
  m_d2dInput.placeholder = placeholder ? placeholder : L"";
  m_d2dInput.maxLength = maxLen;
  m_d2dInput.numbersOnly = numbersOnly;
  m_d2dInput.text = L"";
  m_d2dInput.focused = false; // No focus by default - user must click
  m_d2dInput.cursorPos = 0;
  m_d2dInput.cursorVisible = true;
  m_pInputTextFormat = nullptr;
  m_pPlaceholderFormat = nullptr;
}

void LayoutEngine::DrawD2DInput(ID2D1RenderTarget *pRT, IDWriteFactory *pDWrite,
                                ID2D1SolidColorBrush *pTextBrush,
                                ID2D1SolidColorBrush *pPlaceholderBrush,
                                ID2D1SolidColorBrush *pBorderBrush) {
  if (!pRT || !pDWrite)
    return;

  // Create text format if needed
  if (!m_pInputTextFormat) {
    pDWrite->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL,
                              DWRITE_FONT_STYLE_NORMAL,
                              DWRITE_FONT_STRETCH_NORMAL, m_d2dInput.fontSize,
                              L"en-us", &m_pInputTextFormat);
    if (m_pInputTextFormat) {
      m_pInputTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
      m_pInputTextFormat->SetParagraphAlignment(
          DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
  }

  // Create smaller placeholder format (14pt instead of 18pt)
  if (!m_pPlaceholderFormat) {
    pDWrite->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"fa-IR", &m_pPlaceholderFormat);
    if (m_pPlaceholderFormat) {
      m_pPlaceholderFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
      m_pPlaceholderFormat->SetParagraphAlignment(
          DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
  }

  D2D1_RECT_F inputRect = D2D1::RectF(
      (float)m_d2dInput.bounds.left, (float)m_d2dInput.bounds.top,
      (float)m_d2dInput.bounds.right, (float)m_d2dInput.bounds.bottom);

  // Draw border
  if (pBorderBrush) {
    pRT->DrawRoundedRectangle(D2D1::RoundedRect(inputRect, 6.0f, 6.0f),
                              pBorderBrush, 1.0f);
  }

  // Calculate text area with padding
  float padX = 12.0f;
  float padY = 8.0f;
  D2D1_RECT_F textRect =
      D2D1::RectF(inputRect.left + padX, inputRect.top + padY,
                  inputRect.right - padX, inputRect.bottom - padY);

  // Only show placeholder when NOT focused AND text is empty
  if (m_d2dInput.text.empty() && !m_d2dInput.placeholder.empty() &&
      !m_d2dInput.focused) {
    // Draw placeholder with smaller font, right-aligned
    if (pPlaceholderBrush && m_pPlaceholderFormat) {
      pRT->DrawText(m_d2dInput.placeholder.c_str(),
                    (UINT32)m_d2dInput.placeholder.length(),
                    m_pPlaceholderFormat, textRect, pPlaceholderBrush);
    }
  } else if (!m_d2dInput.text.empty() && pTextBrush) {
    // Draw text with letter-spacing (character by character)
    float x = textRect.left;
    float y = textRect.top;
    float charHeight = textRect.bottom - textRect.top;

    for (size_t i = 0; i < m_d2dInput.text.length(); i++) {
      wchar_t ch[2] = {m_d2dInput.text[i], 0};

      // Create text layout to measure character width
      IDWriteTextLayout *pLayout = nullptr;
      pDWrite->CreateTextLayout(ch, 1, m_pInputTextFormat, 100.0f, charHeight,
                                &pLayout);

      if (pLayout) {
        DWRITE_TEXT_METRICS metrics;
        pLayout->GetMetrics(&metrics);

        D2D1_RECT_F charRect =
            D2D1::RectF(x, y, x + metrics.width, y + charHeight);
        pRT->DrawText(ch, 1, m_pInputTextFormat, charRect, pTextBrush);

        x += metrics.width + m_d2dInput.letterSpacing;
        pLayout->Release();
      }
    }

    // Draw cursor
    if (m_d2dInput.focused && m_d2dInput.cursorVisible) {
      float cursorX = x;
      pRT->DrawLine(D2D1::Point2F(cursorX, textRect.top + 2),
                    D2D1::Point2F(cursorX, textRect.bottom - 2), pTextBrush,
                    1.5f);
    }
  } else if (m_d2dInput.focused && m_d2dInput.cursorVisible && pTextBrush) {
    // Draw cursor when text is empty but focused
    float cursorX = textRect.left;
    pRT->DrawLine(D2D1::Point2F(cursorX, textRect.top + 2),
                  D2D1::Point2F(cursorX, textRect.bottom - 2), pTextBrush,
                  1.5f);
  }
}

bool LayoutEngine::HandleInputChar(WPARAM wParam) {
  if (!m_d2dInput.focused)
    return false;

  wchar_t ch = (wchar_t)wParam;

  // Handle backspace
  if (ch == 8) { // VK_BACK
    if (!m_d2dInput.text.empty()) {
      m_d2dInput.text.pop_back();
      m_d2dInput.cursorPos = (int)m_d2dInput.text.length();
      return true;
    }
    return false;
  }

  // Check max length
  if ((int)m_d2dInput.text.length() >= m_d2dInput.maxLength) {
    return false;
  }

  // Validate character (numbers only if required)
  if (m_d2dInput.numbersOnly) {
    if (ch < L'0' || ch > L'9') {
      return false;
    }
  }

  // Add character
  m_d2dInput.text += ch;
  m_d2dInput.cursorPos = (int)m_d2dInput.text.length();
  return true;
}

bool LayoutEngine::HandleInputClick(int x, int y) {
  POINT pt = {x, y};
  if (PtInRect(&m_d2dInput.bounds, pt)) {
    m_d2dInput.focused = true;
    return true;
  }
  m_d2dInput.focused = false;
  return false;
}

void LayoutEngine::ToggleCursor() {
  m_d2dInput.cursorVisible = !m_d2dInput.cursorVisible;
}

std::wstring LayoutEngine::GetInputText() const { return m_d2dInput.text; }

void LayoutEngine::SetInputText(const std::wstring &text) {
  m_d2dInput.text = text;
  m_d2dInput.cursorPos = (int)text.length();
}

void LayoutEngine::ClearInput() {
  m_d2dInput.text = L"";
  m_d2dInput.cursorPos = 0;
}

bool LayoutEngine::IsInputFocused() const { return m_d2dInput.focused; }

void LayoutEngine::SetInputBounds(RECT bounds) { m_d2dInput.bounds = bounds; }

RECT LayoutEngine::GetInputBounds() const { return m_d2dInput.bounds; }
