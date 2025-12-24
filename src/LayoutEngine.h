#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <map>
#include <string>
#include <vector>
#include <windows.h>

// Custom D2D Input Control with letter-spacing support
struct D2DTextInput {
  RECT bounds;              // Position in parent (logical coords)
  std::wstring text;        // Current text value
  std::wstring placeholder; // Placeholder text
  float letterSpacing;      // Extra space between characters (DIP)
  float fontSize;           // Font size (DIP)
  bool focused;             // Has focus
  int cursorPos;            // Cursor position in text
  bool cursorVisible;       // For blinking animation
  int maxLength;            // Max characters allowed
  bool numbersOnly;         // Only allow digits
};

enum class LayoutType { None, Anchor, Dock };

enum class DockSide { None, Top, Bottom, Left, Right, Fill };

struct AnchorFlags {
  bool left = false;
  bool top = false;
  bool right = false;
  bool bottom = false;
};

struct Margin {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

struct ControlLayout {
  HWND hWnd;
  LayoutType type;

  // Docking props
  DockSide dockSide;
  int fixedSize; // Height for Top/Bottom, Width for Left/Right

  // Anchor props
  AnchorFlags anchors;
  RECT originalRect;       // Snapshotted at registration
  SIZE originalParentSize; // Snapshotted at registration

  // Common
  Margin margin;
  SIZE minSize;
};

class LayoutEngine {
public:
  LayoutEngine();
  ~LayoutEngine();

  void Initialize(HWND hParent);

  // Registration methods
  void RegisterAnchor(HWND hWnd, bool l, bool t, bool r, bool b,
                      Margin m = {0, 0, 0, 0}, SIZE minSz = {0, 0});
  void RegisterDock(HWND hWnd, DockSide side, int size, Margin m = {0, 0, 0, 0},
                    SIZE minSz = {0, 0});

  // Operations
  void Apply(int clientW, int clientH);
  void UpdateDPI(int newDpi, int oldDpi);
  SIZE GetMinTrackSize();

  // Static helper to center a window on screen
  static void CenterWindow(HWND hWnd);

  // Clear all controls (e.g. on recreating UI)
  void Reset();

  // --- D2D Text Input Control ---
  // Create a D2D input control
  void CreateD2DInput(RECT bounds, float letterSpacing, float fontSize,
                      LPCWSTR placeholder, int maxLen = 11,
                      bool numbersOnly = true);

  // Draw the input control (call from OnPaint)
  void DrawD2DInput(ID2D1RenderTarget *pRT, IDWriteFactory *pDWrite,
                    ID2D1SolidColorBrush *pTextBrush,
                    ID2D1SolidColorBrush *pPlaceholderBrush,
                    ID2D1SolidColorBrush *pBorderBrush);

  // Handle keyboard input (call from WM_CHAR)
  bool HandleInputChar(WPARAM wParam);

  // Handle mouse click for focus (call from WM_LBUTTONDOWN)
  bool HandleInputClick(int x, int y);

  // Toggle cursor visibility (call from timer)
  void ToggleCursor();

  // Get/Set text value
  std::wstring GetInputText() const;
  void SetInputText(const std::wstring &text);
  void ClearInput();

  // Check if input has focus
  bool IsInputFocused() const;

  // Update input bounds (for layout changes)
  void SetInputBounds(RECT bounds);
  RECT GetInputBounds() const;

private:
  HWND m_hParent;
  std::vector<ControlLayout> m_controls;
  int m_currentDpi;

  // D2D Input state
  D2DTextInput m_d2dInput;
  IDWriteTextFormat *m_pInputTextFormat;
  IDWriteTextFormat *m_pPlaceholderFormat; // Smaller font for placeholder
};
