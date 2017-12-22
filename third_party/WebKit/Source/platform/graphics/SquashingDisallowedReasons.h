// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SquashingDisallowedReasons_h
#define SquashingDisallowedReasons_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Vector.h"

namespace blink {

using SquashingDisallowedReasons = unsigned;

#define FOR_EACH_SQUASHING_DISALLOWED_REASON(V) \
  V(ScrollsWithRespectToSquashingLayer)         \
  V(SquashingSparsityExceeded)                  \
  V(ClippingContainerMismatch)                  \
  V(OpacityAncestorMismatch)                    \
  V(TransformAncestorMismatch)                  \
  V(FilterMismatch)                             \
  V(WouldBreakPaintOrder)                       \
  V(SquashingVideoIsDisallowed)                 \
  V(SquashedLayerClipsCompositingDescendants)   \
  V(SquashingLayoutEmbeddedContentIsDisallowed) \
  V(SquashingBlendingIsDisallowed)              \
  V(NearestFixedPositionMismatch)               \
  V(ScrollChildWithCompositedDescendants)       \
  V(SquashingLayerIsAnimating)                  \
  V(RenderingContextMismatch)                   \
  V(FragmentedContent)                          \
  V(PrecedingLayerPrecludesSquashing)

class PLATFORM_EXPORT SquashingDisallowedReason {
 private:
  // This contains ordinal values for squashing disallowed reasons and will be
  // used to generate the squashing disallowed reason bits.
  enum {
#define V(name) kE##name,
    FOR_EACH_SQUASHING_DISALLOWED_REASON(V)
#undef V
  };

#define V(name) static_assert(kE##name < 32, "Should fit in 32 bits");
  FOR_EACH_SQUASHING_DISALLOWED_REASON(V)
#undef V

 public:
  static Vector<const char*> ShortNames(SquashingDisallowedReasons);
  static Vector<const char*> Descriptions(SquashingDisallowedReasons);

  enum : SquashingDisallowedReasons {
    kNone = 0,
#define V(name) k##name = 1u << kE##name,
    FOR_EACH_SQUASHING_DISALLOWED_REASON(V)
#undef V
  };
};

}  // namespace blink

#endif  // SquashingDisallowedReasons_h
