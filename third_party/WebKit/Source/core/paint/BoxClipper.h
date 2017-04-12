// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxClipper_h
#define BoxClipper_h

#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"

namespace blink {

class LayoutBox;
struct PaintInfo;

enum ContentsClipBehavior { kForceContentsClip, kSkipContentsClipIfPossible };

class BoxClipper {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  BoxClipper(const LayoutBox&,
             const PaintInfo&,
             const LayoutPoint& accumulated_offset,
             ContentsClipBehavior);
  ~BoxClipper();

 private:
  const LayoutBox& box_;
  const PaintInfo& paint_info_;
  DisplayItem::Type clip_type_;

  Optional<ScopedPaintChunkProperties> scoped_clip_property_;
};

}  // namespace blink

#endif  // BoxClipper_h
