// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxDecorationData.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/style/BorderEdge.h"
#include "core/layout/style/LayoutStyle.h"
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

    m_bleedAvoidance = determineBackgroundBleedAvoidance(layoutBox, context);
}

BackgroundBleedAvoidance BoxDecorationData::determineBackgroundBleedAvoidance(const LayoutBox& layoutBox, GraphicsContext* context)
{
    if (layoutBox.isDocumentElement())
        return BackgroundBleedNone;

    if (!hasBackground)
        return BackgroundBleedNone;

    if (!hasBorder || !layoutBox.style()->hasBorderRadius() || layoutBox.canRenderBorderImage()) {
        if (layoutBox.backgroundShouldAlwaysBeClipped())
            return BackgroundBleedClipBackground;
        return BackgroundBleedNone;
    }

    // If display lists are enabled (via Slimming Paint), then simply clip the background and do not
    // perform advanced bleed-avoidance heuristics. These heuristics are not correct in the presence
    // of impl-side rasterization or layerization, since the actual pixel-relative scaling and rotation
    // of the content is not known to Blink.
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        return BackgroundBleedClipBackground;

    // FIXME: See crbug.com/382491. getCTM does not accurately reflect the scale at the time content is
    // rasterized, and should not be relied on to make decisions about bleeding.
    AffineTransform ctm = context->getCTM();
    FloatSize contextScaling(static_cast<float>(ctm.xScale()), static_cast<float>(ctm.yScale()));

    // Because RoundedRect uses IntRect internally the inset applied by the
    // BackgroundBleedShrinkBackground strategy cannot be less than one integer
    // layout coordinate, even with subpixel layout enabled. To take that into
    // account, we clamp the contextScaling to 1.0 for the following test so
    // that borderObscuresBackgroundEdge can only return true if the border
    // widths are greater than 2 in both layout coordinates and screen
    // coordinates.
    // This precaution will become obsolete if RoundedRect is ever promoted to
    // a sub-pixel representation.
    if (contextScaling.width() > 1)
        contextScaling.setWidth(1);
    if (contextScaling.height() > 1)
        contextScaling.setHeight(1);

    if (borderObscuresBackgroundEdge(*layoutBox.style(), contextScaling))
        return BackgroundBleedShrinkBackground;
    if (!hasAppearance && layoutBox.style()->borderObscuresBackground() && layoutBox.backgroundHasOpaqueTopLayer())
        return BackgroundBleedBackgroundOverBorder;

    return BackgroundBleedClipBackground;
}

bool BoxDecorationData::borderObscuresBackgroundEdge(const LayoutStyle& style, const FloatSize& contextScale) const
{
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

} // namespace blink
