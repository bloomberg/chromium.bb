// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterPainter_h
#define FilterPainter_h

#include <memory>
#include "core/paint/PaintLayerPaintingInfo.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ClipRect;
class GraphicsContext;
class PaintLayer;
class LayerClipRecorder;
class LayoutObject;

class FilterPainter {
 public:
  FilterPainter(PaintLayer&,
                GraphicsContext&,
                const LayoutPoint& offset_from_root,
                const ClipRect&,
                PaintLayerPaintingInfo&,
                PaintLayerFlags paint_flags);
  ~FilterPainter();

  // Returns whether it's ok to clip this PaintLayer's painted outputs
  // the dirty rect. Some filters require input from outside this rect, in
  // which case this method would return true.
  static sk_sp<SkImageFilter> GetImageFilter(PaintLayer&);

 private:
  bool filter_in_progress_;
  GraphicsContext& context_;
  std::unique_ptr<LayerClipRecorder> clip_recorder_;
  LayoutObject& layout_object_;
};

}  // namespace blink

#endif  // FilterPainter_h
