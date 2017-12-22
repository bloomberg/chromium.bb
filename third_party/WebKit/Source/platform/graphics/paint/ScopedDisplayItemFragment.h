// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedDisplayItemFragment_h
#define ScopedDisplayItemFragment_h

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class ScopedDisplayItemFragment final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(ScopedDisplayItemFragment);

 public:
  ScopedDisplayItemFragment(GraphicsContext& context, unsigned fragment)
      : context_(context),
        original_fragment_(context.GetPaintController().CurrentFragment()) {
    context.GetPaintController().SetCurrentFragment(fragment);
  }
  ~ScopedDisplayItemFragment() {
    context_.GetPaintController().SetCurrentFragment(original_fragment_);
  }

 private:
  GraphicsContext& context_;
  unsigned original_fragment_;
};

}  // namespace blink

#endif  // ScopedDisplayItemFragment_h
