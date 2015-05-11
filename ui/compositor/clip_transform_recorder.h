// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CLIP_TRANSFORM_RECORDER_H_
#define UI_COMPOSITOR_CLIP_TRANSFORM_RECORDER_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/compositor_export.h"

namespace cc {
class DisplayItem;
class DisplayItemList;
}

namespace gfx {
class Path;
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
  explicit ClipTransformRecorder(const PaintContext& context);
  ~ClipTransformRecorder();

  void ClipRect(const gfx::Rect& clip_rect);
  void ClipPath(const gfx::Path& clip_path);
  void ClipPathWithAntiAliasing(const gfx::Path& clip_path);
  void Transform(const gfx::Transform& transform);

 private:
  enum Closer {
    CLIP_RECT,
    CLIP_PATH,
    TRANSFORM,
  };
  const PaintContext& context_;
  // If someone needs to do more than this many operations with a single
  // ClipTransformRecorder then increase the size of the closers_ array.
  Closer closers_[4];
  size_t num_closers_;

  DISALLOW_COPY_AND_ASSIGN(ClipTransformRecorder);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CLIP_TRANSFORM_RECORDER_H_
