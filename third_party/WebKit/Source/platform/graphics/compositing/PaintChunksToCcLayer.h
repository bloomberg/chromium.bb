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
class RasterInvalidationTracking;

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
  // Converts a list of Blink paint chunks and display items into cc display
  // items, inserting appropriate begin/end items with respect to property
  // tree state. The converted items are appended into a unfinalized cc display
  // item list.
  // |layer_state| is the target property tree state of the output. This method
  // generates begin/end items for the relative state differences between the
  // layer state and the chunk state.
  // |layer_offset| is an extra translation on top of layer_state.Transform(),
  // in other word, point (x, y) in the output list maps to
  // layer_state.Transform() * (layer_offset + (x, y)) on the screen. It is
  // equivalent to say that |layer_offset| is the layer origin in the space
  // of layer_state.Transform().
  static void ConvertInto(const Vector<const PaintChunk*>&,
                          const PropertyTreeState& layer_state,
                          const gfx::Vector2dF& layer_offset,
                          const DisplayItemList&,
                          cc::DisplayItemList&);

  // Similar to ConvertInto(), but returns a finalized new list instead of
  // appending converted items to an existing list.
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
