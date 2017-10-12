// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/SquashingDisallowedReasons.h"

#include "platform/wtf/StdLibExtras.h"

namespace blink {

const SquashingDisallowedReasonStringMap kSquashingDisallowedReasonStringMap[] =
    {
        {kSquashingDisallowedReasonScrollsWithRespectToSquashingLayer,
         "scrollsWithRespectToSquashingLayer",
         "Cannot be squashed since this layer scrolls with respect to the "
         "squashing layer"},
        {kSquashingDisallowedReasonSquashingSparsityExceeded,
         "squashingSparsityExceeded",
         "Cannot be squashed as the squashing layer would become too sparse"},
        {kSquashingDisallowedReasonClippingContainerMismatch,
         "squashingClippingContainerMismatch",
         "Cannot be squashed because this layer has a different clipping "
         "container than the squashing layer"},
        {kSquashingDisallowedReasonOpacityAncestorMismatch,
         "squashingOpacityAncestorMismatch",
         "Cannot be squashed because this layer has a different opacity "
         "ancestor than the squashing layer"},
        {kSquashingDisallowedReasonTransformAncestorMismatch,
         "squashingTransformAncestorMismatch",
         "Cannot be squashed because this layer has a different transform "
         "ancestor than the squashing layer"},
        {kSquashingDisallowedReasonFilterMismatch,
         "squashingFilterAncestorMismatch",
         "Cannot be squashed because this layer has a different filter "
         "ancestor than the squashing layer, or this layer has a filter"},
        {kSquashingDisallowedReasonWouldBreakPaintOrder,
         "squashingWouldBreakPaintOrder",
         "Cannot be squashed without breaking paint order"},
        {kSquashingDisallowedReasonSquashingVideoIsDisallowed,
         "squashingVideoIsDisallowed", "Squashing video is not supported"},
        {kSquashingDisallowedReasonSquashedLayerClipsCompositingDescendants,
         "squashedLayerClipsCompositingDescendants",
         "Squashing a layer that clips composited descendants is not "
         "supported."},
        {kSquashingDisallowedReasonSquashingLayoutEmbeddedContentIsDisallowed,
         "squashingLayoutEmbeddedContentIsDisallowed",
         "Squashing a frame, iframe or plugin is not supported."},
        {kSquashingDisallowedReasonSquashingBlendingIsDisallowed,
         "squashingBlendingDisallowed",
         "Squashing a layer with blending is not supported."},
        {kSquashingDisallowedReasonNearestFixedPositionMismatch,
         "squashingNearestFixedPositionMismatch",
         "Cannot be squashed because this layer has a different nearest fixed "
         "position layer than the squashing layer"},
        {kSquashingDisallowedReasonScrollChildWithCompositedDescendants,
         "scrollChildWithCompositedDescendants",
         "Squashing a scroll child with composited descendants is not "
         "supported."},
        {kSquashingDisallowedReasonSquashingLayerIsAnimating,
         "squashingLayerIsAnimating",
         "Cannot squash into a layer that is animating."},
        {kSquashingDisallowedReasonRenderingContextMismatch,
         "squashingLayerRenderingContextMismatch",
         "Cannot squash layers with different 3D contexts."},
        {kSquashingDisallowedReasonFragmentedContent,
         "SquashingDisallowedReasonFragmentedContent",
         "Cannot squash layers that are inside fragmentation contexts."},
        {kSquashingDisallowedReasonBorderRadiusClipsDescendants,
         "SquashingDisallowedReasonBorderRadiusClipsDecendants",
         "Cannot squash layers that must apply a border radius clip to their "
         "decendants"},
};

const size_t kNumberOfSquashingDisallowedReasons =
    WTF_ARRAY_LENGTH(kSquashingDisallowedReasonStringMap);

}  // namespace blink
