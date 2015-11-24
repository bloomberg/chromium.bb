// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CLIP_RECORDER_H_
#define UI_COMPOSITOR_CLIP_RECORDER_H_

#include "base/macros.h"
#include "ui/compositor/compositor_export.h"

namespace cc {
class DisplayItem;
class DisplayItemList;
}

namespace gfx {
class Path;
class Rect;
}

namespace ui {
class PaintContext;

// A class to provide scoped clips of painting to a DisplayItemList. The clip
// provided will be applied to any DisplayItems added to the DisplayItemList
// while this object is alive. In other words, any nested PaintRecorders or
// other ClipRecorders will be clipped.
class COMPOSITOR_EXPORT ClipRecorder {
 public:
  explicit ClipRecorder(const PaintContext& context);
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
  // If someone needs to do more than this many operations with a single
  // ClipRecorder then increase the size of the closers_ array.
  Closer closers_[4];
  size_t num_closers_;

  DISALLOW_COPY_AND_ASSIGN(ClipRecorder);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CLIP_RECORDER_H_
