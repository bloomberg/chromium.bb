// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DISPLAY_ITEM_LIST_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DISPLAY_ITEM_LIST_H_

#include "third_party/blink/public/platform/web_blend_mode.h"
#include "third_party/blink/public/platform/web_vector.h"

#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorFilter;
class SkMatrix44;
class SkPath;
struct SkRect;
class SkRRect;

namespace cc {
class DisplayItemList;
class FilterOperations;
}

namespace gfx {
class Rect;
class RectF;
class Vector2d;
}  // namespace gfx

namespace blink {

// An ordered list of items representing content to be rendered (stored in
// 'drawing' items) and operations to be performed when rendering this content
// (stored in 'clip', 'transform', 'filter', etc...). For more details see:
// http://dev.chromium.org/blink/slimming-paint.
class WebDisplayItemList {
 public:
  virtual ~WebDisplayItemList() = default;

  virtual void AppendDrawingItem(const gfx::Rect& visual_rect,
                                 sk_sp<const cc::PaintRecord>) {}

  virtual void AppendClipItem(const gfx::Rect& clip_rect,
                              const WebVector<SkRRect>& rounded_clip_rects) {}
  virtual void AppendEndClipItem() {}
  virtual void AppendClipPathItem(const SkPath&, bool antialias) {}
  virtual void AppendEndClipPathItem() {}
  virtual void AppendFloatClipItem(const gfx::RectF& clip_rect) {}
  virtual void AppendEndFloatClipItem() {}
  virtual void AppendTransformItem(const SkMatrix44&) {}
  virtual void AppendEndTransformItem() {}
  // If bounds is provided, the content will be explicitly clipped to those
  // bounds.
  virtual void AppendCompositingItem(float opacity,
                                     SkBlendMode,
                                     SkRect* bounds,
                                     SkColorFilter*) {}
  virtual void AppendEndCompositingItem() {}

  // TODO(loyso): This should use CompositorFilterOperation. crbug.com/584551
  virtual void AppendFilterItem(const cc::FilterOperations&,
                                const gfx::RectF& filter_bounds,
                                const gfx::PointF& origin) {}
  virtual void AppendEndFilterItem() {}

  // Scroll containers are identified by an opaque pointer.
  using ScrollContainerId = const void*;
  virtual void AppendScrollItem(const gfx::Vector2d& scroll_offset,
                                ScrollContainerId) {}
  virtual void AppendEndScrollItem() {}

  virtual cc::DisplayItemList* GetCcDisplayItemList() { return nullptr; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DISPLAY_ITEM_LIST_H_
