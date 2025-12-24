#include "HeaderPopup.h"
#include "Icons.h"
#include <d2d1_3.h>
#include <dwrite.h>
#include <functional>
#include <string>
#include <vector>
#include <windows.h>

// HeaderBar - Custom D2D-rendered header that looks like web UI
// Features: shadow, rounded corners, hover states, RTL text, flexbox-like
// layout

// Forward declarations
class HeaderBar;

// Callback types
using HeaderCallback = std::function<void()>;
using DropdownCallback = std::function<void(int index)>;

// Header item types
enum class HeaderItemType {
  Logo,     // Red rounded rect with "دیوار" text
  Dropdown, // City/Category selector with arrow
  Spacer,   // Flexible space (flex-grow)
  Button,   // Action button (Start, Stop, Export)
  UserArea, // User icon + "دیوار من" + logout
  Divider   // Vertical separator line
};

// Header item structure
enum class HeaderAlignment { Left, Right };

struct HeaderItem {
  HeaderItemType type;
  HeaderAlignment alignment = HeaderAlignment::Right; // Default RTL
  std::wstring text;
  HeaderIcon iconType = HeaderIcon::None; // Vector icon
  bool isHovered = false;
  bool isPressed = false;
  bool isEnabled = true;
  float x = 0, y = 0, w = 0, h = 0;
  HeaderCallback onClick;
  DropdownCallback onSelect;
  std::vector<std::wstring> dropdownItems;
  int selectedIndex = 0;
  COLORREF textColor = 0x707070;
  COLORREF hoverColor = 0x00C853;
};

class HeaderBar {
public:
  // Initialization
  HeaderBar(HWND hParent);
  ~HeaderBar();

  // Initialization
  static bool RegisterClass(HINSTANCE hInstance); // Keep static
  HWND Create(HINSTANCE hInstance);               // Instance method

  // Item management
  void AddItem(const HeaderItem &item);
  void ClearItems();
  void SetItemEnabled(int index, bool enabled);
  void SetDropdownItems(int itemIndex, const std::vector<std::wstring> &items);
  void SetDropdownSelection(int itemIndex, int selection);
  int GetDropdownSelection(int itemIndex) const;
  void SetItemText(int itemIndex, const std::wstring &text);
  void SetItemIcon(int itemIndex, HeaderIcon icon);
  void SetItemColor(int itemIndex, COLORREF color);

  // Public for layout forcing
  void OnSize(int width, int height);

  // Show a dropdown popup for a specific  // Dropdown
  void
  ShowDropdown(int itemIndex, const std::vector<MenuItem> &options,
               std::function<void(int, std::wstring, std::string)> onSelect,
               bool alignRight = false);

  // Layout
  void SetHeight(int height) { m_height = height; }
  void SetPadding(int padding) { m_padding = padding; }
  void SetGap(int gap) { m_gap = gap; }
  void UpdateLayout();

  // Rendering
  void Invalidate();

  // Get HWND
  HWND GetHwnd() const { return m_hwnd; }

private:
  // D2D Resources
  ID2D1Factory3 *m_pD2DFactory = nullptr;
  ID2D1HwndRenderTarget *m_pRenderTarget = nullptr;
  IDWriteFactory *m_pDWriteFactory = nullptr;
  IDWriteTextFormat *m_pTextFormat = nullptr;
  IDWriteTextFormat *m_pLogoFormat = nullptr;

  // Brushes
  ID2D1SolidColorBrush *m_pWhiteBrush = nullptr;
  ID2D1SolidColorBrush *m_pBorderBrush = nullptr;
  ID2D1SolidColorBrush *m_pRedBrush = nullptr;
  ID2D1SolidColorBrush *m_pGrayBrush = nullptr;
  ID2D1SolidColorBrush *m_pHoverBrush = nullptr;
  ID2D1SolidColorBrush *m_pTextBrush = nullptr;

  // Window
  HWND m_hwnd = nullptr;
  HWND m_parent = nullptr;
  int m_width = 0;
  int m_height = 64;
  int m_padding = 16;
  int m_gap = 12;

  // Items
  std::vector<HeaderItem> m_items;
  int m_hoveredItem = -1;
  int m_pressedItem = -1;

  // Dropdown popup
  HWND m_dropdownPopup = nullptr;
  int m_activeDropdown = -1;

  // Internal methods
  HRESULT CreateDeviceResources();
  void DiscardDeviceResources();
  void OnPaint();
  // OnSize moved to public
  void OnMouseMove(int x, int y);
  void OnMouseLeave();
  void OnLButtonDown(int x, int y);
  void OnLButtonUp(int x, int y);
  int HitTest(int x, int y);

  // Drawing helpers
  void DrawLogo(const HeaderItem &item);
  void DrawDropdown(const HeaderItem &item);
  void DrawButton(const HeaderItem &item);
  void DrawUserArea(const HeaderItem &item);
  void DrawDivider(const HeaderItem &item);
  void DrawIcon(HeaderIcon icon, D2D1_RECT_F rect, ID2D1Brush *brush);
  void DrawShadow();

  // Window procedure
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
  LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance (for WndProc access)
extern HeaderBar *g_HeaderBar;
