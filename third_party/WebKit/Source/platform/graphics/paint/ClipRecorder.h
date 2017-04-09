// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT ClipRecorder {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(ClipRecorder);

 public:
  ClipRecorder(GraphicsContext&,
               const DisplayItemClient&,
               DisplayItem::Type,
               const IntRect& clip_rect);
  ~ClipRecorder();

 private:
  const DisplayItemClient& client_;
  GraphicsContext& context_;
  DisplayItem::Type type_;
};

}  // namespace blink

#endif  // ClipRecorder_h
