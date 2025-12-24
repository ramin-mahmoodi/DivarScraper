#pragma once
#include "Icons.h"
#include <algorithm>
#include <d2d1_3.h>
#include <dwrite.h>
#include <string>
#include <vector>


class DrawHelper {
public:
  static void DrawIcon(ID2D1RenderTarget *pRT, ID2D1Factory3 *pFactory,
                       HeaderIcon icon, D2D1_RECT_F rect, ID2D1Brush *brush);
  static void ParseSVGPath(ID2D1GeometrySink *pSink, const char *path);
};
