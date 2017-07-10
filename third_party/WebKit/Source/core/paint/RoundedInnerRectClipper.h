// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RoundedInnerRectClipper_h
#define RoundedInnerRectClipper_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class FloatRoundedRect;
class LayoutRect;
class DisplayItemClient;
struct PaintInfo;

enum RoundedInnerRectClipperBehavior { kApplyToDisplayList, kApplyToContext };

class RoundedInnerRectClipper {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  RoundedInnerRectClipper(const DisplayItemClient&,
                          const PaintInfo&,
                          const LayoutRect&,
                          const FloatRoundedRect& clip_rect,
                          RoundedInnerRectClipperBehavior);
  ~RoundedInnerRectClipper();

 private:
  const DisplayItemClient& display_item_;
  const PaintInfo& paint_info_;
  bool use_paint_controller_;
  DisplayItem::Type clip_type_;
};

}  // namespace blink

#endif  // RoundedInnerRectClipper_h
