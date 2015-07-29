// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_vector_icon.h"

#include <map>

#include "base/lazy_instance.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icons2.h"

namespace gfx {

namespace {

class VectorIconSource : public CanvasImageSource {
 public:
  VectorIconSource(VectorIconId id, size_t dip_size, SkColor color)
      : CanvasImageSource(
            gfx::Size(static_cast<int>(dip_size), static_cast<int>(dip_size)),
            false),
        id_(id),
        color_(color) {}

  ~VectorIconSource() override {}

  // CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    PaintVectorIcon(canvas, id_, size_.width(), color_);
  }

 private:
  const VectorIconId id_;
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(VectorIconSource);
};

// This class caches vector icons (as ImageSkia) so they don't have to be drawn
// more than once. This also guarantees the backing data for the images returned
// by CreateVectorIcon will persist in memory until program termination.
class VectorIconCache {
 public:
  VectorIconCache() {}
  ~VectorIconCache() {}

  ImageSkia GetOrCreateIcon(VectorIconId id, size_t dip_size, SkColor color) {
    IconDescription description(id, dip_size, color);
    auto iter = images_.find(description);
    if (iter != images_.end())
      return iter->second;

    ImageSkia icon(
        new VectorIconSource(id, dip_size, color),
        gfx::Size(static_cast<int>(dip_size), static_cast<int>(dip_size)));
    images_.insert(std::make_pair(description, icon));
    return icon;
  }

 private:
  struct IconDescription {
    IconDescription(VectorIconId id, size_t dip_size, SkColor color)
        : id(id), dip_size(dip_size), color(color) {}

    bool operator<(const IconDescription& other) const {
      if (id != other.id)
        return id < other.id;
      if (dip_size != other.dip_size)
        return dip_size < other.dip_size;
      return color < other.color;
    }

    VectorIconId id;
    size_t dip_size;
    SkColor color;
  };

  std::map<IconDescription, ImageSkia> images_;

  DISALLOW_COPY_AND_ASSIGN(VectorIconCache);
};

static base::LazyInstance<VectorIconCache> g_icon_cache =
    LAZY_INSTANCE_INITIALIZER;

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

ImageSkia CreateVectorIcon(VectorIconId id, size_t dip_size, SkColor color) {
  return g_icon_cache.Get().GetOrCreateIcon(id, dip_size, color);
}

}  // namespace gfx
