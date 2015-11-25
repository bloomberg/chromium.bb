// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TRANSFORM_RECORDER_H_
#define UI_COMPOSITOR_TRANSFORM_RECORDER_H_

#include "base/macros.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class DisplayItem;
class DisplayItemList;
}

namespace gfx {
class Size;
class Transform;
}

namespace ui {
class PaintContext;

// A class to provide scoped transforms of painting to a DisplayItemList. The
// transform provided will be applied to any DisplayItems added to the
// DisplayItemList while this object is alive. In other words, any nested
// PaintRecorders or other TransformRecorders will be transformed.
class COMPOSITOR_EXPORT TransformRecorder {
 public:
  // |size_in_layer| is the size in layer space dimensions surrounding
  // everything that's visible.
  TransformRecorder(const PaintContext& context,
                    const gfx::Size& size_in_layer);
  ~TransformRecorder();

  void Transform(const gfx::Transform& transform);

 private:
  const PaintContext& context_;
  const gfx::Rect bounds_in_layer_;
  bool transformed_;

  DISALLOW_COPY_AND_ASSIGN(TransformRecorder);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TRANSFORM_RECORDER_H_
