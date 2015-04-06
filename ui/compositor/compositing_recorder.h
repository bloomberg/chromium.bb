// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITING_RECORDER_H_
#define UI_COMPOSITOR_COMPOSITING_RECORDER_H_

#include "base/macros.h"
#include "ui/compositor/compositor_export.h"

namespace gfx {
class Canvas;
}

namespace ui {
class PaintContext;

// A class to provide scoped compositing filters (eg opacity) of painting to a
// DisplayItemList. The filters provided will be applied to any
// DisplayItems added to the DisplayItemList while this object is alive. In
// other words, any nested PaintRecorders or other such Recorders will
// be filtered by the effect.
class COMPOSITOR_EXPORT CompositingRecorder {
 public:
  CompositingRecorder(const PaintContext& context, float opacity);
  ~CompositingRecorder();

 private:
  gfx::Canvas* canvas_;

  DISALLOW_COPY_AND_ASSIGN(CompositingRecorder);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CLIP_TRANSFORM_RECORDER_H_
