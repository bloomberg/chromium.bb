// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxDecorationData.h"

#include "core/rendering/style/BorderEdge.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

BoxDecorationData::BoxDecorationData(const RenderStyle& style, bool canRenderBorderImage, bool backgroundHasOpaqueTopLayer, GraphicsContext* context)
{
    backgroundColor = style.visitedDependentColor(CSSPropertyBackgroundColor);
    hasBackground = backgroundColor.alpha() || style.hasBackgroundImage();
    ASSERT(hasBackground == style.hasBackground());
    hasBorder = style.hasBorder();
    hasAppearance = style.hasAppearance();

    m_bleedAvoidance = determineBackgroundBleedAvoidance(style, canRenderBorderImage, backgroundHasOpaqueTopLayer, context);
}

BackgroundBleedAvoidance BoxDecorationData::determineBackgroundBleedAvoidance(const RenderStyle& style, bool canRenderBorderImage, bool backgroundHasOpaqueTopLayer, GraphicsContext* context)
{
    if (!hasBackground || !hasBorder || !style.hasBorderRadius() || canRenderBorderImage)
        return BackgroundBleedNone;

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

    if (borderObscuresBackgroundEdge(style, contextScaling))
        return BackgroundBleedShrinkBackground;
    if (!hasAppearance && style.borderObscuresBackground() && backgroundHasOpaqueTopLayer)
        return BackgroundBleedBackgroundOverBorder;

    return BackgroundBleedClipBackground;
}

bool BoxDecorationData::borderObscuresBackgroundEdge(const RenderStyle& style, const FloatSize& contextScale) const
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
