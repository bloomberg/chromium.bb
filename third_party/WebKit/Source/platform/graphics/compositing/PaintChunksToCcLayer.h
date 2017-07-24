// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunksToCcLayer_h
#define PaintChunksToCcLayer_h

#include "base/memory/ref_counted.h"
#include "cc/paint/display_item_list.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace cc {
class DisplayItemList;
}  // namespace cc

namespace gfx {
class Vector2dF;
}  // namespace gfx

namespace blink {

class DisplayItemList;
struct PaintChunk;
class PropertyTreeState;
struct RasterInvalidationTracking;

struct RasterUnderInvalidationCheckingParams {
  RasterUnderInvalidationCheckingParams(RasterInvalidationTracking& tracking,
                                        const IntRect& interest_rect,
                                        const String& debug_name)
      : tracking(tracking),
        interest_rect(interest_rect),
        debug_name(debug_name) {}

  RasterInvalidationTracking& tracking;
  IntRect interest_rect;
  String debug_name;
};

class PLATFORM_EXPORT PaintChunksToCcLayer {
 public:
  static scoped_refptr<cc::DisplayItemList> Convert(
      const Vector<const PaintChunk*>&,
      const PropertyTreeState& layer_state,
      const gfx::Vector2dF& layer_offset,
      const DisplayItemList&,
      cc::DisplayItemList::UsageHint,
      RasterUnderInvalidationCheckingParams* = nullptr);
};

}  // namespace blink

#endif  // PaintArtifactCompositor_h
