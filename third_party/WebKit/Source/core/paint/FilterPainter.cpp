// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FilterPainter.h"

#include "core/layout/FilterEffectRenderer.h"
#include "core/layout/Layer.h"
#include "core/layout/LayoutView.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/LayerPainter.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/FilterDisplayItem.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFilterOperations.h"

namespace blink {

FilterPainter::FilterPainter(Layer& renderLayer, GraphicsContext* context, const LayoutPoint& offsetFromRoot, const ClipRect& clipRect, LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags,
    LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
    : m_filterInProgress(false)
    , m_context(context)
    , m_renderer(renderLayer.layoutObject())
{
    if (!renderLayer.filterRenderer() || !renderLayer.paintsWithFilters())
        return;

    ASSERT(renderLayer.filterInfo());

    SkiaImageFilterBuilder builder(context);
    RefPtrWillBeRawPtr<FilterEffect> lastEffect = renderLayer.filterRenderer()->lastEffect();
    lastEffect->determineFilterPrimitiveSubregion(MapRectForward);
    RefPtr<SkImageFilter> imageFilter = builder.build(lastEffect.get(), ColorSpaceDeviceRGB);
    if (!imageFilter)
        return;

    if (!rootRelativeBoundsComputed) {
        rootRelativeBounds = renderLayer.physicalBoundingBoxIncludingReflectionAndStackingChildren(paintingInfo.rootLayer, offsetFromRoot);
        rootRelativeBoundsComputed = true;
    }

    // We'll handle clipping to the dirty rect before filter rasterization.
    // Filter processing will automatically expand the clip rect and the offscreen to accommodate any filter outsets.
    // FIXME: It is incorrect to just clip to the damageRect here once multiple fragments are involved.

    // Subsequent code should not clip to the dirty rect, since we've already
    // done it above, and doing it later will defeat the outsets.
    paintingInfo.clipToDirtyRect = false;

    if (clipRect.rect() != paintingInfo.paintDirtyRect || clipRect.hasRadius()) {
        m_clipRecorder = adoptPtr(new LayerClipRecorder(renderLayer.layoutObject(), context, DisplayItem::ClipLayerFilter, clipRect, &paintingInfo, LayoutPoint(), paintFlags));
    }

    ASSERT(m_renderer);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        FilterOperations filterOperations(renderLayer.computeFilterOperations(m_renderer->styleRef()));
        OwnPtr<WebFilterOperations> webFilterOperations = adoptPtr(Platform::current()->compositorSupport()->createFilterOperations());
        builder.buildFilterOperations(filterOperations, webFilterOperations.get());
        OwnPtr<BeginFilterDisplayItem> filterDisplayItem = BeginFilterDisplayItem::create(m_renderer->displayItemClient(), imageFilter, rootRelativeBounds, webFilterOperations.release());

        ASSERT(context->displayItemList());
        context->displayItemList()->add(filterDisplayItem.release());
    } else {
        OwnPtr<BeginFilterDisplayItem> filterDisplayItem = BeginFilterDisplayItem::create(m_renderer->displayItemClient(), imageFilter, rootRelativeBounds);

        filterDisplayItem->replay(context);
    }

    m_filterInProgress = true;
}

FilterPainter::~FilterPainter()
{
    if (!m_filterInProgress)
        return;

    OwnPtr<EndFilterDisplayItem> endFilterDisplayItem = EndFilterDisplayItem::create(m_renderer->displayItemClient());
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context->displayItemList());
        m_context->displayItemList()->add(endFilterDisplayItem.release());
    } else {
        endFilterDisplayItem->replay(m_context);
    }
}

} // namespace blink
