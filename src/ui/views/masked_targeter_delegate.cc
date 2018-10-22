// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/masked_targeter_delegate.h"

#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"

namespace views {

bool MaskedTargeterDelegate::DoesIntersectRect(const View* target,
                                               const gfx::Rect& rect) const {
  // Early return if |rect| does not even intersect the rectangular bounds
  // of |target|.
  if (!ViewTargeterDelegate::DoesIntersectRect(target, rect))
    return false;

  // Early return if |mask| is not a valid hit test mask.
  gfx::Path mask;
  if (!GetHitTestMask(&mask))
    return false;

  // Return whether or not |rect| intersects the custom hit test mask
  // of |target|.
  SkRegion clip_region;
  clip_region.setRect(0, 0, target->width(), target->height());
  SkRegion mask_region;
  return mask_region.setPath(mask, clip_region) &&
         mask_region.intersects(RectToSkIRect(rect));
}

}  // namespace views
