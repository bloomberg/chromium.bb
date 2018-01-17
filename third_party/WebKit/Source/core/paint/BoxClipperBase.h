// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxClipperBase_h
#define BoxClipperBase_h

#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"

namespace blink {

class DisplayItemClient;
class FragmentData;
struct PaintInfo;

class BoxClipperBase {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 protected:
  void InitializeScopedClipProperty(const FragmentData*,
                                    const DisplayItemClient&,
                                    const PaintInfo&);

  Optional<ScopedPaintChunkProperties> scoped_clip_property_;
};

}  // namespace blink

#endif  // BoxClipper_h
