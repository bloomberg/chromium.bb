// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxDecorationData.h"

#include "core/layout/LayoutBox.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

BoxDecorationData::BoxDecorationData(const LayoutBox& layoutBox, GraphicsContext* context)
{
    backgroundColor = layoutBox.style()->visitedDependentColor(CSSPropertyBackgroundColor);
    hasBackground = backgroundColor.alpha() || layoutBox.style()->hasBackgroundImage();
    ASSERT(hasBackground == layoutBox.style()->hasBackground());
    hasBorder = layoutBox.style()->hasBorder();
    hasAppearance = layoutBox.style()->hasAppearance();
    bleedAvoidance = determineBackgroundBleedAvoidance(layoutBox, context);
}

namespace {

bool borderObscuresBackgroundEdge(const GraphicsContext& context, const ComputedStyle& style)
{
    // FIXME: See crbug.com/382491. getCTM does not accurately reflect the scale at the time content is
    // rasterized, and should not be relied on to make decisions about bleeding.
    AffineTransform ctm = context.getCTM();
    FloatSize contextScale(static_cast<float>(ctm.xScale()), static_cast<float>(ctm.yScale()));

    // Because RoundedRect uses IntRect internally the inset applied by the
    // BackgroundBleedShrinkBackground strategy cannot be less than one integer
    // layout coordinate, even with subpixel layout enabled. To take that into
    // account, we clamp the contextScaling to 1.0 for the following test so
    // that borderObscuresBackgroundEdge can only return true if the border
    // widths are greater than 2 in both layout coordinates and screen
    // coordinates.
    // This precaution will become obsolete if RoundedRect is ever promoted to
    // a sub-pixel representation.
    if (contextScale.width() > 1)
        contextScale.setWidth(1);
    if (contextScale.height() > 1)
        contextScale.setHeight(1);

    BorderEdge edges[4];
    style.getBorderEdgeInfo(edges);

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];
        // FIXME: for vertical text
        float axisScale = (i == BSTop || i == BSBottom) ? contextScale.height() : contextScale.width();
        if (!currEdge.obscuresBackgroundEdge(axisScale))
            return false;
    }

    return true;
}

} // anonymous namespace

BackgroundBleedAvoidance BoxDecorationData::determineBackgroundBleedAvoidance(const LayoutBox& layoutBox, GraphicsContext* context)
{
    if (layoutBox.isDocumentElement())
        return BackgroundBleedNone;

    if (!hasBackground)
        return BackgroundBleedNone;

    if (!hasBorder || !layoutBox.style()->hasBorderRadius() || layoutBox.canRenderBorderImage()) {
        if (layoutBox.backgroundShouldAlwaysBeClipped())
            return BackgroundBleedClipOnly;
        return BackgroundBleedNone;
    }

    // If display lists are enabled (via Slimming Paint), then BackgroundBleedShrinkBackground is not
    // usable as is relies on device-space heuristics. These heuristics are not correct in the presence
    // of impl-side rasterization or layerization, since the actual pixel-relative scaling and rotation
    // of the content is not known to Blink.
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled() && borderObscuresBackgroundEdge(*context, *layoutBox.style()))
        return BackgroundBleedShrinkBackground;

    if (!hasAppearance && layoutBox.style()->borderObscuresBackground() && layoutBox.backgroundHasOpaqueTopLayer())
        return BackgroundBleedBackgroundOverBorder;

    return BackgroundBleedClipLayer;
}

} // namespace blink
