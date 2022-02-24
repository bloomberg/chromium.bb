// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_

#include <stdint.h>
#include <vector>
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CompositingReasons = uint64_t;

#define FOR_EACH_COMPOSITING_REASON(V)                                        \
  /* Intrinsic reasons that can be known right away by the layer. */          \
  V(3DTransform)                                                              \
  V(Trivial3DTransform)                                                       \
  V(Video)                                                                    \
  V(Canvas)                                                                   \
  V(Plugin)                                                                   \
  V(IFrame)                                                                   \
  V(DocumentTransitionPseudoElement)                                          \
  V(BackfaceVisibilityHidden)                                                 \
  V(ActiveTransformAnimation)                                                 \
  V(ActiveOpacityAnimation)                                                   \
  V(ActiveFilterAnimation)                                                    \
  V(ActiveBackdropFilterAnimation)                                            \
  V(AffectedByOuterViewportBoundsDelta)                                       \
  V(FixedPosition)                                                            \
  V(StickyPosition)                                                           \
  V(OverflowScrolling)                                                        \
  V(WillChangeTransform)                                                      \
  V(WillChangeOpacity)                                                        \
  V(WillChangeFilter)                                                         \
  V(WillChangeBackdropFilter)                                                 \
                                                                              \
  /* Reasons that depend on ancestor properties */                            \
  V(BackfaceInvisibility3DAncestor)                                           \
  /* TODO(crbug.com/1256990): Transform3DSceneLeaf today depends only on the  \
     element and its properties, but in the future it could be optimized      \
     to consider descendants and moved to the subtree group below. */         \
  V(Transform3DSceneLeaf)                                                     \
  /* This flag is needed only when none of the explicit kWillChange* reasons  \
     are set. */                                                              \
  V(WillChangeOther)                                                          \
  V(BackdropFilter)                                                           \
  V(BackdropFilterMask)                                                       \
  V(RootScroller)                                                             \
  V(Viewport)                                                                 \
                                                                              \
  /* This is based on overlapping relationship among pending layers,          \
     determined after paint. See PaintArtifactCompositor. */                  \
  V(Overlap)                                                                  \
                                                                              \
  /* Subtree reasons that require knowing what the status of your subtree is  \
     before knowing the answer. */                                            \
  V(OpacityWithCompositedDescendants)                                         \
  V(MaskWithCompositedDescendants)                                            \
  V(FilterWithCompositedDescendants)                                          \
  V(BlendingWithCompositedDescendants)                                        \
  V(PerspectiveWith3DDescendants)                                             \
  V(Preserve3DWith3DDescendants)                                              \
                                                                              \
  /* The root layer is a special case. It may be forced to be a layer, but it \
  also needs to be a layer if anything else in the subtree is composited. */  \
  V(Root)                                                                     \
                                                                              \
  V(LayerForHorizontalScrollbar)                                              \
  V(LayerForVerticalScrollbar)                                                \
  /* Link highlight, frame overlay, etc. */                                   \
  V(LayerForOther)                                                            \
                                                                              \
  /* DocumentTransition shared element.                                       \
  See third_party/blink/renderer/core/document_transition/README.md. */       \
  V(DocumentTransitionSharedElement)

class PLATFORM_EXPORT CompositingReason {
  DISALLOW_NEW();

 private:
  // This contains ordinal values for compositing reasons and will be used to
  // generate the compositing reason bits.
  enum {
#define V(name) kE##name,
    FOR_EACH_COMPOSITING_REASON(V)
#undef V
  };

#define V(name) static_assert(kE##name < 64, "Should fit in 64 bits");
  FOR_EACH_COMPOSITING_REASON(V)
#undef V

 public:
  static std::vector<const char*> ShortNames(CompositingReasons);
  static std::vector<const char*> Descriptions(CompositingReasons);
  static String ToString(CompositingReasons);

  enum : CompositingReasons {
    kNone = 0,
    kAll = ~static_cast<CompositingReasons>(0),
#define V(name) k##name = UINT64_C(1) << kE##name,
    FOR_EACH_COMPOSITING_REASON(V)
#undef V

    // Various combinations of compositing reasons are defined here also, for
    // more intuitive and faster bitwise logic.
    kComboActiveAnimation =
        kActiveTransformAnimation | kActiveOpacityAnimation |
        kActiveFilterAnimation | kActiveBackdropFilterAnimation,
    kComboScrollDependentPosition = kFixedPosition | kStickyPosition,
    kPreventingSubpixelAccumulationReasons = kWillChangeTransform,
    kDirectReasonsForPaintOffsetTranslationProperty =
        kComboScrollDependentPosition | kAffectedByOuterViewportBoundsDelta |
        kVideo | kCanvas | kPlugin | kIFrame,
    kDirectReasonsForTransformProperty =
        k3DTransform | kTrivial3DTransform | kWillChangeTransform |
        kWillChangeOther | kPerspectiveWith3DDescendants |
        kPreserve3DWith3DDescendants | kActiveTransformAnimation,
    kDirectReasonsForScrollTranslationProperty =
        kRootScroller | kOverflowScrolling,
    kDirectReasonsForEffectProperty =
        kActiveOpacityAnimation | kWillChangeOpacity | kBackdropFilter |
        kWillChangeBackdropFilter | kActiveBackdropFilterAnimation |
        kDocumentTransitionPseudoElement | kTransform3DSceneLeaf,
    kDirectReasonsForFilterProperty =
        kActiveFilterAnimation | kWillChangeFilter,
    kDirectReasonsForBackdropFilter = kBackdropFilter |
                                      kActiveBackdropFilterAnimation |
                                      kWillChangeBackdropFilter,
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_
