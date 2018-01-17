// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxClipper_h
#define BoxClipper_h

#include "core/paint/BoxClipperBase.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class LayoutBox;

enum ContentsClipBehavior { kForceContentsClip, kSkipContentsClipIfPossible };

class BoxClipper : public BoxClipperBase {
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
};

}  // namespace blink

#endif  // BoxClipper_h
