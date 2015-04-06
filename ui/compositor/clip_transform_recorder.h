// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CLIP_TRANSFORM_RECORDER_H_
#define UI_COMPOSITOR_CLIP_TRANSFORM_RECORDER_H_

#include "base/macros.h"
#include "ui/compositor/compositor_export.h"

namespace gfx {
class Canvas;
class Rect;
class Transform;
}

namespace ui {
class PaintContext;

// A class to provide scoped clips and transforms of painting to a
// DisplayItemList. The the clip/transform provided will be applied to any
// DisplayItems added to the DisplayItemList while this object is alive. In
// other words, any nested PaintRecorders or other ClipTransformRecorders will
// be clipped/transformed.
class COMPOSITOR_EXPORT ClipTransformRecorder {
 public:
  ClipTransformRecorder(const PaintContext& context,
                        const gfx::Rect& clip_rect);
  ClipTransformRecorder(const PaintContext& context,
                        const gfx::Transform& transform);
  ClipTransformRecorder(const PaintContext& context,
                        const gfx::Rect& clip_rect,
                        const gfx::Transform& transform);
  ~ClipTransformRecorder();

 private:
  gfx::Canvas* canvas_;

  DISALLOW_COPY_AND_ASSIGN(ClipTransformRecorder);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CLIP_TRANSFORM_RECORDER_H_
