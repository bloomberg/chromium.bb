// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CLIP_RECORDER_H_
#define UI_COMPOSITOR_CLIP_RECORDER_H_

#include "base/macros.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class DisplayItem;
class DisplayItemList;
}

namespace gfx {
class Path;
class Size;
}

namespace ui {
class PaintContext;

// A class to provide scoped clips of painting to a DisplayItemList. The clip
// provided will be applied to any DisplayItems added to the DisplayItemList
// while this object is alive. In other words, any nested PaintRecorders or
// other ClipRecorders will be clipped.
class COMPOSITOR_EXPORT ClipRecorder {
 public:
  // |size_in_layer| is the size in layer space dimensions surrounding
  // everything that's visible.
  ClipRecorder(const PaintContext& context, const gfx::Size& size_in_layer);
  ~ClipRecorder();

  void ClipRect(const gfx::Rect& clip_rect);
  void ClipPath(const gfx::Path& clip_path);
  void ClipPathWithAntiAliasing(const gfx::Path& clip_path);

 private:
  enum Closer {
    CLIP_RECT,
    CLIP_PATH,
  };
  const PaintContext& context_;
  const gfx::Rect bounds_in_layer_;
  // If someone needs to do more than this many operations with a single
  // ClipRecorder then increase the size of the closers_ array.
  Closer closers_[4];
  size_t num_closers_;

  DISALLOW_COPY_AND_ASSIGN(ClipRecorder);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CLIP_RECORDER_H_
