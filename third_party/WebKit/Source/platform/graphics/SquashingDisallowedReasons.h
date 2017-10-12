// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SquashingDisallowedReasons_h
#define SquashingDisallowedReasons_h

#include <stdint.h>
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

enum SquashingDisallowedReason {
  kSquashingDisallowedReasonsNone = 0,
  kSquashingDisallowedReasonScrollsWithRespectToSquashingLayer = 1 << 0,
  kSquashingDisallowedReasonSquashingSparsityExceeded = 1 << 1,
  kSquashingDisallowedReasonClippingContainerMismatch = 1 << 2,
  kSquashingDisallowedReasonOpacityAncestorMismatch = 1 << 3,
  kSquashingDisallowedReasonTransformAncestorMismatch = 1 << 4,
  kSquashingDisallowedReasonFilterMismatch = 1 << 5,
  kSquashingDisallowedReasonWouldBreakPaintOrder = 1 << 6,
  kSquashingDisallowedReasonSquashingVideoIsDisallowed = 1 << 7,
  kSquashingDisallowedReasonSquashedLayerClipsCompositingDescendants = 1 << 8,
  kSquashingDisallowedReasonSquashingLayoutEmbeddedContentIsDisallowed = 1 << 9,
  kSquashingDisallowedReasonSquashingBlendingIsDisallowed = 1 << 10,
  kSquashingDisallowedReasonNearestFixedPositionMismatch = 1 << 11,
  kSquashingDisallowedReasonScrollChildWithCompositedDescendants = 1 << 12,
  kSquashingDisallowedReasonSquashingLayerIsAnimating = 1 << 13,
  kSquashingDisallowedReasonRenderingContextMismatch = 1 << 14,
  kSquashingDisallowedReasonFragmentedContent = 1 << 15,
  kSquashingDisallowedReasonBorderRadiusClipsDescendants = 1 << 16,
};

typedef unsigned SquashingDisallowedReasons;

struct SquashingDisallowedReasonStringMap {
  STACK_ALLOCATED();
  SquashingDisallowedReasons reason;
  const char* short_name;
  const char* description;
};

PLATFORM_EXPORT extern const SquashingDisallowedReasonStringMap
    kSquashingDisallowedReasonStringMap[];
PLATFORM_EXPORT extern const size_t kNumberOfSquashingDisallowedReasons;

}  // namespace blink

#endif  // SquashingDisallowedReasons_h
