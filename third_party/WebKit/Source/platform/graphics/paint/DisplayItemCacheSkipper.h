// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemCacheSkipper_h
#define DisplayItemCacheSkipper_h

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class DisplayItemCacheSkipper final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(DisplayItemCacheSkipper);

 public:
  DisplayItemCacheSkipper(GraphicsContext& context) : context_(context) {
    context.GetPaintController().BeginSkippingCache();
  }
  ~DisplayItemCacheSkipper() {
    context_.GetPaintController().EndSkippingCache();
  }

 private:
  GraphicsContext& context_;
};

}  // namespace blink

#endif  // DisplayItemCacheSkipper_h
