#pragma once
#include "CategoryDB.h" // Defines MenuItem
#include "Icons.h"
#include <d2d1_3.h>
#include <dwrite.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

class HeaderPopup {
public:
  // Show the popup. Returns handle if needed.
  // Using MenuItem to support nested menus
  static HWND Show(HINSTANCE hInst, HWND parent, int x, int y, int w,
                   const std::vector<MenuItem> &options,
                   std::function<void(int, std::wstring, std::string)> onSelect,
                   HWND hParentPopup = nullptr, bool alignRight = false);

  static void RegisterWndClass(HINSTANCE hInstance);

private:
  HeaderPopup(HWND hwnd, const std::vector<MenuItem> &options,
              std::function<void(int, std::wstring, std::string)> onSelect);
  ~HeaderPopup();

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // D2D
  void OnPaint();
  void OnSize(int width, int height);
  void OnMouseMove(int x, int y);
  void OnLButtonUp(int x, int y);

  // Submenu logic
  void ShowSubMenu(int index);
  void CloseSubMenu();

  HRESULT CreateDeviceResources();
  void DiscardDeviceResources();

  HWND m_hwnd;
  HWND m_hSubMenu = nullptr;     // Track SubMenu
  HWND m_hParentPopup = nullptr; // Track Parent Message
  std::vector<MenuItem> m_options;
  std::function<void(int, std::wstring, std::string)> m_selectCallback;

  // D2D Resources
  static ID2D1Factory3 *m_pD2DFactory;
  static IDWriteFactory *m_pDWriteFactory;

  ID2D1HwndRenderTarget *m_pRenderTarget = nullptr;
  ID2D1SolidColorBrush *m_pWhiteBrush = nullptr;
  ID2D1SolidColorBrush *m_pTextBrush = nullptr;
  ID2D1SolidColorBrush *m_pHoverBrush = nullptr; // Light gray for hover
  ID2D1SolidColorBrush *m_pBorderBrush = nullptr;
  ID2D1SolidColorBrush *m_pTitleBgBrush = nullptr;
  IDWriteTextFormat *m_pTextFormat = nullptr;

  int m_hoveredIndex = -1;
  int m_itemHeight = 32;
  int m_width = 0;
  int m_height = 0;

  // Scroll state
  float m_scrollOffset = 0.0f;
  float m_maxScroll = 0.0f;
};
