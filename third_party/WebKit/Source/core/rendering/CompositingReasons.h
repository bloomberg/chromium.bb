// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasons_h
#define CompositingReasons_h

#include "wtf/MathExtras.h"

namespace WebCore {

const uint64_t CompositingReasonNone                                   = 0;

// Intrinsic reasons that can be known right away by the layer
const uint64_t CompositingReason3DTransform                            = UINT64_C(1) << 0;
const uint64_t CompositingReasonVideo                                  = UINT64_C(1) << 1;
const uint64_t CompositingReasonCanvas                                 = UINT64_C(1) << 2;
const uint64_t CompositingReasonPlugin                                 = UINT64_C(1) << 3;
const uint64_t CompositingReasonIFrame                                 = UINT64_C(1) << 4;
const uint64_t CompositingReasonBackfaceVisibilityHidden               = UINT64_C(1) << 5;
const uint64_t CompositingReasonAnimation                              = UINT64_C(1) << 6;
const uint64_t CompositingReasonFilters                                = UINT64_C(1) << 7;
const uint64_t CompositingReasonPositionFixed                          = UINT64_C(1) << 8;
const uint64_t CompositingReasonPositionSticky                         = UINT64_C(1) << 9;
const uint64_t CompositingReasonOverflowScrollingTouch                 = UINT64_C(1) << 10;
const uint64_t CompositingReasonBlending                               = UINT64_C(1) << 11;

// Overlap reasons that require knowing what's behind you in paint-order before knowing the answer
const uint64_t CompositingReasonAssumedOverlap                         = UINT64_C(1) << 12;
const uint64_t CompositingReasonOverlap                                = UINT64_C(1) << 13;
const uint64_t CompositingReasonNegativeZIndexChildren                 = UINT64_C(1) << 14;

// Subtree reasons that require knowing what the status of your subtree is before knowing the answer
const uint64_t CompositingReasonTransformWithCompositedDescendants     = UINT64_C(1) << 15;
const uint64_t CompositingReasonOpacityWithCompositedDescendants       = UINT64_C(1) << 16;
const uint64_t CompositingReasonMaskWithCompositedDescendants          = UINT64_C(1) << 17;
const uint64_t CompositingReasonReflectionWithCompositedDescendants    = UINT64_C(1) << 18;
const uint64_t CompositingReasonFilterWithCompositedDescendants        = UINT64_C(1) << 19;
const uint64_t CompositingReasonBlendingWithCompositedDescendants      = UINT64_C(1) << 20;
const uint64_t CompositingReasonClipsCompositingDescendants            = UINT64_C(1) << 21;
const uint64_t CompositingReasonPerspective                            = UINT64_C(1) << 22;
const uint64_t CompositingReasonPreserve3D                             = UINT64_C(1) << 23;
const uint64_t CompositingReasonReflectionOfCompositedParent           = UINT64_C(1) << 24;

// The root layer is a special case that may be forced to be a layer, but also it needs to be
// a layer if anything else in the subtree is composited.
const uint64_t CompositingReasonRoot                                   = UINT64_C(1) << 25;

// RenderLayerBacking internal hierarchy reasons
const uint64_t CompositingReasonLayerForClip                           = UINT64_C(1) << 26;
const uint64_t CompositingReasonLayerForScrollbar                      = UINT64_C(1) << 27;
const uint64_t CompositingReasonLayerForScrollingContainer             = UINT64_C(1) << 28;
const uint64_t CompositingReasonLayerForForeground                     = UINT64_C(1) << 29;
const uint64_t CompositingReasonLayerForBackground                     = UINT64_C(1) << 30;
const uint64_t CompositingReasonLayerForMask                           = UINT64_C(1) << 31;

// FIXME: the following compositing reasons need to be re-organized to fit with categories
// used in all the other reasons above.
const uint64_t CompositingReasonLayerForVideoOverlay                   = UINT64_C(1) << 32;

// Note: if you add more reasons here, you will need to update WebCompositingReasons as well.
typedef uint64_t CompositingReasons;


} // namespace WebCore

#endif // CompositingReasons_h
