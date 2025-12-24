#include "DrawHelper.h"
#include <cmath>

void DrawHelper::ParseSVGPath(ID2D1GeometrySink *pSink, const char *path) {
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
      D2D1_POINT_2F p1 = {2 * cur.x - lastCtrl.x, 2 * cur.y - lastCtrl.y};
      D2D1_POINT_2F p2 = {ReadFloat(), ReadFloat()};
      cur = {ReadFloat(), ReadFloat()};
      pSink->AddBezier(D2D1::BezierSegment(p1, p2, cur));
      lastCtrl = p2;
      break;
    }
    case 's': {
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
    default:
      ReadFloat();
      break;
    }
  }
}

void DrawHelper::DrawIcon(ID2D1RenderTarget *pRT, ID2D1Factory3 *pFactory,
                          HeaderIcon icon, D2D1_RECT_F rect,
                          ID2D1Brush *brush) {
  if (icon == HeaderIcon::None)
    return;

  ID2D1PathGeometry *pPath = nullptr;
  pFactory->CreatePathGeometry(&pPath);
  if (!pPath)
    return;

  ID2D1GeometrySink *pSink = nullptr;
  pPath->Open(&pSink);
  pSink->SetFillMode(D2D1_FILL_MODE_WINDING);

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
  } else if (icon == HeaderIcon::Logout) {
    // Simplified Logout Icon (Door + Arrow)
    d = "M9 21H5V3h4 M16 17l5-5l-5-5 M21 12H9";
  } else if (icon == HeaderIcon::User) {
    // Lucide User Icon (Stroke-friendly, custom figure logic below)
  }

  if (icon == HeaderIcon::User) {
    // Head Circle
    pSink->BeginFigure(D2D1::Point2F(16, 7), D2D1_FIGURE_BEGIN_HOLLOW);
    pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(8, 7), D2D1::SizeF(4, 4), 0,
                                   D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                   D2D1_ARC_SIZE_LARGE));
    pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(16, 7), D2D1::SizeF(4, 4), 0,
                                   D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                   D2D1_ARC_SIZE_LARGE));
    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);

    // Body Path
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
  } else {
    ParseSVGPath(pSink, d);
  }

  pSink->Close();
  pSink->Release();

  float w = rect.right - rect.left;
  float h = rect.bottom - rect.top;
  float scale = (std::min)(w / 24.0f, h / 24.0f);
  float tx = rect.left + (w - 24.0f * scale) / 2.0f;
  float ty = rect.top + (h - 24.0f * scale) / 2.0f;

  D2D1::Matrix3x2F oldTransform;
  pRT->GetTransform(&oldTransform);
  pRT->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                    D2D1::Matrix3x2F::Translation(tx, ty) * oldTransform);

  if (icon == HeaderIcon::User || icon == HeaderIcon::Logout) {
    pRT->DrawGeometry(pPath, brush, 2.0f);
  } else {
    pRT->FillGeometry(pPath, brush);
  }
  pRT->SetTransform(oldTransform);

  pPath->Release();
}
