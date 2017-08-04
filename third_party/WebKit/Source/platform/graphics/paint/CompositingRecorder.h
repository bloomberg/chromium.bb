// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingRecorder_h
#define CompositingRecorder_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Allocator.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT CompositingRecorder {
  USING_FAST_MALLOC(CompositingRecorder);

 public:
  // If bounds is provided, the content will be explicitly clipped to those
  // bounds.
  CompositingRecorder(GraphicsContext&,
                      const DisplayItemClient&,
                      const SkBlendMode,
                      const float opacity,
                      const FloatRect* bounds = nullptr,
                      ColorFilter = kColorFilterNone);

  ~CompositingRecorder();

 private:
  const DisplayItemClient& client_;
  GraphicsContext& graphics_context_;
};

}  // namespace blink

#endif  // CompositingRecorder_h
