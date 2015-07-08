// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/vector_icons.h"

#include "ui/gfx/canvas.h"

namespace gfx {

namespace {

const int kReferenceSizeDip = 48;

enum CommandType {
  MOVE_TO,
  R_MOVE_TO,
  LINE_TO,
  R_LINE_TO,
  H_LINE_TO,
  R_H_LINE_TO,
  V_LINE_TO,
  R_V_LINE_TO,
  R_CUBIC_TO,
  CIRCLE,
  CLOSE,
  END
};

struct PathElement {
  PathElement(CommandType value) : type(value) {}
  PathElement(SkScalar value) : arg(value) {}

  union {
    CommandType type;
    SkScalar arg;
  };
};

const PathElement* GetPathForVectorIcon(VectorIconId id) {
  switch (id) {
    case VectorIconId::VECTOR_ICON_NONE:
      NOTREACHED();
      return nullptr;

    case VectorIconId::CHECK_CIRCLE: {
      static PathElement path[] = {
        CIRCLE, 24, 24, 20,
        MOVE_TO, 20, 34,
        LINE_TO, 10, 24,
        R_LINE_TO, 2.83f, -2.83f,
        LINE_TO, 20, 28.34f,
        R_LINE_TO, 15.17f, -15.17f,
        LINE_TO, 38, 16,
        LINE_TO, 20, 34,
        END
      };
      return path;
    }

    case VectorIconId::PHOTO_CAMERA: {
      static PathElement path[] = {
        CIRCLE, 24, 24, 6.4f,
        MOVE_TO, 18, 4,
        R_LINE_TO, -3.66f, 4,
        H_LINE_TO, 8,
        R_CUBIC_TO, -2.21f, 0, -4, 1.79f, -4, 4,
        R_V_LINE_TO, 24,
        R_CUBIC_TO, 0, 2.21f, 1.79f, 4, 4, 4,
        R_H_LINE_TO, 32,
        R_CUBIC_TO, 2.21f, 0, 4, -1.79f, 4, -4,
        V_LINE_TO, 12,
        R_CUBIC_TO, 0, -2.21f, -1.79f, -4, -4, -4,
        R_H_LINE_TO, -6.34f,
        LINE_TO, 30, 4,
        H_LINE_TO, 18,
        CLOSE,
        CIRCLE, 24, 24, 10,
        END
      };
      return path;
    }
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace

void PaintVectorIcon(Canvas* canvas,
                     VectorIconId id,
                     size_t dip_size,
                     SkColor color) {
  DCHECK(VectorIconId::VECTOR_ICON_NONE != id);
  const PathElement* path_elements = GetPathForVectorIcon(id);
  SkPath path;
  path.setFillType(SkPath::kEvenOdd_FillType);
  if (dip_size != kReferenceSizeDip) {
    SkScalar scale = SkIntToScalar(dip_size) / SkIntToScalar(kReferenceSizeDip);
    canvas->sk_canvas()->scale(scale, scale);
  }

  for (size_t i = 0; path_elements[i].type != END;) {
    switch (path_elements[i++].type) {
      case MOVE_TO: {
        SkScalar x = path_elements[i++].arg;
        SkScalar y = path_elements[i++].arg;
        path.moveTo(x, y);
        break;
      }

      case R_MOVE_TO: {
        SkScalar x = path_elements[i++].arg;
        SkScalar y = path_elements[i++].arg;
        path.rMoveTo(x, y);
        break;
      }

      case LINE_TO: {
        SkScalar x = path_elements[i++].arg;
        SkScalar y = path_elements[i++].arg;
        path.lineTo(x, y);
        break;
      }

      case R_LINE_TO: {
        SkScalar x = path_elements[i++].arg;
        SkScalar y = path_elements[i++].arg;
        path.rLineTo(x, y);
        break;
      }

      case H_LINE_TO: {
        SkPoint last_point;
        path.getLastPt(&last_point);
        SkScalar x = path_elements[i++].arg;
        path.lineTo(x, last_point.fY);
        break;
      }

      case R_H_LINE_TO: {
        SkScalar x = path_elements[i++].arg;
        path.rLineTo(x, 0);
        break;
      }

      case V_LINE_TO: {
        SkPoint last_point;
        path.getLastPt(&last_point);
        SkScalar y = path_elements[i++].arg;
        path.lineTo(last_point.fX, y);
        break;
      }

      case R_V_LINE_TO: {
        SkScalar y = path_elements[i++].arg;
        path.rLineTo(0, y);
        break;
      }

      case R_CUBIC_TO: {
        SkScalar x1 = path_elements[i++].arg;
        SkScalar y1 = path_elements[i++].arg;
        SkScalar x2 = path_elements[i++].arg;
        SkScalar y2 = path_elements[i++].arg;
        SkScalar x3 = path_elements[i++].arg;
        SkScalar y3 = path_elements[i++].arg;
        path.rCubicTo(x1, y1, x2, y2, x3, y3);
        break;
      }

      case CLOSE: {
        path.close();
        break;
      }

      case CIRCLE: {
        SkScalar x = path_elements[i++].arg;
        SkScalar y = path_elements[i++].arg;
        SkScalar r = path_elements[i++].arg;
        path.addCircle(x, y, r);
        break;
      }

      case END:
        NOTREACHED();
        break;
    }
  }

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setColor(color);
  canvas->DrawPath(path, paint);
}

}  // namespace gfx
