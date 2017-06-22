// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasons_h
#define CompositingReasons_h

#include <stdint.h>
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

const uint64_t kCompositingReasonNone = 0;
const uint64_t kCompositingReasonAll = ~static_cast<uint64_t>(0);

// Intrinsic reasons that can be known right away by the layer
const uint64_t kCompositingReason3DTransform = UINT64_C(1) << 0;
const uint64_t kCompositingReasonVideo = UINT64_C(1) << 1;
const uint64_t kCompositingReasonCanvas = UINT64_C(1) << 2;
const uint64_t kCompositingReasonPlugin = UINT64_C(1) << 3;
const uint64_t kCompositingReasonIFrame = UINT64_C(1) << 4;
const uint64_t kCompositingReasonBackfaceVisibilityHidden = UINT64_C(1) << 5;
const uint64_t kCompositingReasonActiveAnimation = UINT64_C(1) << 6;
const uint64_t kCompositingReasonTransitionProperty = UINT64_C(1) << 7;
const uint64_t kCompositingReasonScrollDependentPosition = UINT64_C(1) << 8;
const uint64_t kCompositingReasonOverflowScrollingTouch = UINT64_C(1) << 9;
const uint64_t kCompositingReasonOverflowScrollingParent = UINT64_C(1) << 10;
const uint64_t kCompositingReasonOutOfFlowClipping = UINT64_C(1) << 11;
const uint64_t kCompositingReasonVideoOverlay = UINT64_C(1) << 12;
const uint64_t kCompositingReasonWillChangeCompositingHint = UINT64_C(1) << 13;
const uint64_t kCompositingReasonBackdropFilter = UINT64_C(1) << 14;
const uint64_t kCompositingReasonRootScroller = UINT64_C(1) << 15;

// Overlap reasons that require knowing what's behind you in paint-order before
// knowing the answer.
const uint64_t kCompositingReasonAssumedOverlap = UINT64_C(1) << 16;
const uint64_t kCompositingReasonOverlap = UINT64_C(1) << 17;
const uint64_t kCompositingReasonNegativeZIndexChildren = UINT64_C(1) << 18;
const uint64_t kCompositingReasonSquashingDisallowed = UINT64_C(1) << 19;

// Subtree reasons that require knowing what the status of your subtree is
// before knowing the answer.
const uint64_t kCompositingReasonTransformWithCompositedDescendants =
    UINT64_C(1) << 20;
const uint64_t kCompositingReasonOpacityWithCompositedDescendants = UINT64_C(1)
                                                                    << 21;
const uint64_t kCompositingReasonMaskWithCompositedDescendants = UINT64_C(1)
                                                                 << 22;
const uint64_t kCompositingReasonReflectionWithCompositedDescendants =
    UINT64_C(1) << 23;
const uint64_t kCompositingReasonFilterWithCompositedDescendants = UINT64_C(1)
                                                                   << 24;
const uint64_t kCompositingReasonBlendingWithCompositedDescendants = UINT64_C(1)
                                                                     << 25;
const uint64_t kCompositingReasonClipsCompositingDescendants = UINT64_C(1)
                                                               << 26;
const uint64_t kCompositingReasonPerspectiveWith3DDescendants = UINT64_C(1)
                                                                << 27;
const uint64_t kCompositingReasonPreserve3DWith3DDescendants = UINT64_C(1)
                                                               << 28;
const uint64_t kCompositingReasonReflectionOfCompositedParent = UINT64_C(1)
                                                                << 29;
const uint64_t kCompositingReasonIsolateCompositedDescendants = UINT64_C(1)
                                                                << 30;
const uint64_t
    kCompositingReasonPositionFixedOrStickyWithCompositedDescendants =
        UINT64_C(1) << 31;

// The root layer is a special case. It may be forced to be a layer, but it also
// needs to be a layer if anything else in the subtree is composited.
const uint64_t kCompositingReasonRoot = UINT64_C(1) << 32;

// CompositedLayerMapping internal hierarchy reasons
const uint64_t kCompositingReasonLayerForAncestorClip = UINT64_C(1) << 33;
const uint64_t kCompositingReasonLayerForDescendantClip = UINT64_C(1) << 34;
const uint64_t kCompositingReasonLayerForPerspective = UINT64_C(1) << 35;
const uint64_t kCompositingReasonLayerForHorizontalScrollbar = UINT64_C(1)
                                                               << 36;
const uint64_t kCompositingReasonLayerForVerticalScrollbar = UINT64_C(1) << 37;
const uint64_t kCompositingReasonLayerForOverflowControlsHost = UINT64_C(1)
                                                                << 38;
const uint64_t kCompositingReasonLayerForScrollCorner = UINT64_C(1) << 39;
const uint64_t kCompositingReasonLayerForScrollingContents = UINT64_C(1) << 40;
const uint64_t kCompositingReasonLayerForScrollingContainer = UINT64_C(1) << 41;
const uint64_t kCompositingReasonLayerForSquashingContents = UINT64_C(1) << 42;
const uint64_t kCompositingReasonLayerForSquashingContainer = UINT64_C(1) << 43;
const uint64_t kCompositingReasonLayerForForeground = UINT64_C(1) << 44;
const uint64_t kCompositingReasonLayerForBackground = UINT64_C(1) << 45;
const uint64_t kCompositingReasonLayerForMask = UINT64_C(1) << 46;
const uint64_t kCompositingReasonLayerForClippingMask = UINT64_C(1) << 47;
const uint64_t kCompositingReasonLayerForAncestorClippingMask = UINT64_C(1)
                                                                << 48;
const uint64_t kCompositingReasonLayerForScrollingBlockSelection = UINT64_C(1)
                                                                   << 49;
// Composited layer painted on top of all other layers as decoration
const uint64_t kCompositingReasonLayerForDecoration = UINT64_C(1) << 50;

// Composited elements with inline transforms trigger assumed overlap so that
// we can update their transforms quickly.
const uint64_t kCompositingReasonInlineTransform = UINT64_C(1) << 51;

// Various combinations of compositing reasons are defined here also, for more
// intutive and faster bitwise logic.
const uint64_t kCompositingReasonComboAllDirectReasons =
    kCompositingReason3DTransform | kCompositingReasonVideo |
    kCompositingReasonCanvas | kCompositingReasonPlugin |
    kCompositingReasonIFrame | kCompositingReasonBackfaceVisibilityHidden |
    kCompositingReasonActiveAnimation | kCompositingReasonTransitionProperty |
    kCompositingReasonScrollDependentPosition |
    kCompositingReasonOverflowScrollingTouch |
    kCompositingReasonOverflowScrollingParent |
    kCompositingReasonOutOfFlowClipping | kCompositingReasonVideoOverlay |
    kCompositingReasonWillChangeCompositingHint |
    kCompositingReasonBackdropFilter | kCompositingReasonRootScroller;

const uint64_t kCompositingReasonComboAllDirectStyleDeterminedReasons =
    kCompositingReason3DTransform | kCompositingReasonBackfaceVisibilityHidden |
    kCompositingReasonActiveAnimation | kCompositingReasonTransitionProperty |
    kCompositingReasonWillChangeCompositingHint |
    kCompositingReasonBackdropFilter;

const uint64_t kCompositingReasonComboCompositedDescendants =
    kCompositingReasonTransformWithCompositedDescendants |
    kCompositingReasonIsolateCompositedDescendants |
    kCompositingReasonOpacityWithCompositedDescendants |
    kCompositingReasonMaskWithCompositedDescendants |
    kCompositingReasonFilterWithCompositedDescendants |
    kCompositingReasonBlendingWithCompositedDescendants |
    kCompositingReasonReflectionWithCompositedDescendants |
    kCompositingReasonClipsCompositingDescendants |
    kCompositingReasonPositionFixedOrStickyWithCompositedDescendants;

const uint64_t kCompositingReasonCombo3DDescendants =
    kCompositingReasonPreserve3DWith3DDescendants |
    kCompositingReasonPerspectiveWith3DDescendants;

const uint64_t kCompositingReasonComboAllStyleDeterminedReasons =
    kCompositingReasonComboAllDirectStyleDeterminedReasons |
    kCompositingReasonComboCompositedDescendants |
    kCompositingReasonCombo3DDescendants | kCompositingReasonInlineTransform;

const uint64_t kCompositingReasonComboReasonsThatRequireOwnBacking =
    kCompositingReasonComboAllDirectReasons | kCompositingReasonOverlap |
    kCompositingReasonAssumedOverlap |
    kCompositingReasonNegativeZIndexChildren |
    kCompositingReasonSquashingDisallowed |
    kCompositingReasonTransformWithCompositedDescendants |
    kCompositingReasonOpacityWithCompositedDescendants |
    kCompositingReasonMaskWithCompositedDescendants |
    kCompositingReasonFilterWithCompositedDescendants |
    kCompositingReasonBlendingWithCompositedDescendants |
    kCompositingReasonIsolateCompositedDescendants |
    kCompositingReasonPreserve3DWith3DDescendants |  // preserve-3d has to
                                                     // create a backing store
                                                     // to ensure that
                                                     // 3d-transformed elements
                                                     // intersect.
    kCompositingReasonBackdropFilter |
    kCompositingReasonPositionFixedOrStickyWithCompositedDescendants;

const uint64_t kCompositingReasonComboSquashableReasons =
    kCompositingReasonOverlap | kCompositingReasonAssumedOverlap |
    kCompositingReasonOverflowScrollingParent;

typedef uint64_t CompositingReasons;

// Any reasons other than overlap or assumed overlap will require the layer to
// be separately compositing.
inline bool RequiresCompositing(CompositingReasons reasons) {
  return reasons & ~kCompositingReasonComboSquashableReasons;
}

// If the layer has overlap or assumed overlap, but no other reasons, then it
// should be squashed.
inline bool RequiresSquashing(CompositingReasons reasons) {
  return !RequiresCompositing(reasons) &&
         (reasons & kCompositingReasonComboSquashableReasons);
}

struct CompositingReasonStringMap {
  STACK_ALLOCATED();
  CompositingReasons reason;
  const char* short_name;
  const char* description;
};

PLATFORM_EXPORT extern const CompositingReasonStringMap
    kCompositingReasonStringMap[];
PLATFORM_EXPORT extern const size_t kNumberOfCompositingReasons;
PLATFORM_EXPORT String CompositingReasonsAsString(CompositingReasons);

}  // namespace blink

#endif  // CompositingReasons_h
