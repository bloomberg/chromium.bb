// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FramePainter.h"

#include "core/dom/DocumentMarkerController.h"
#include "core/frame/FrameView.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/paint/LayerPainter.h"
#include "core/paint/ScrollbarPainter.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

bool FramePainter::s_inPaintContents = false;

void FramePainter::paint(GraphicsContext* context, const IntRect& rect)
{
    m_frameView.notifyPageThatContentAreaWillPaint();

    IntRect documentDirtyRect = rect;
    IntRect visibleAreaWithoutScrollbars(m_frameView.location(), m_frameView.visibleContentRect().size());
    documentDirtyRect.intersect(visibleAreaWithoutScrollbars);

    if (!documentDirtyRect.isEmpty()) {
        GraphicsContextStateSaver stateSaver(*context);

        context->translate(m_frameView.x() - m_frameView.scrollX(), m_frameView.y() - m_frameView.scrollY());
        context->clip(m_frameView.visibleContentRect());

        documentDirtyRect.moveBy(-m_frameView.location() + m_frameView.scrollPosition());
        paintContents(context, documentDirtyRect);
    }

    calculateAndPaintOverhangAreas(context, rect);

    // Now paint the scrollbars.
    if (!m_frameView.scrollbarsSuppressed() && (m_frameView.horizontalScrollbar() || m_frameView.verticalScrollbar())) {
        GraphicsContextStateSaver stateSaver(*context);
        IntRect scrollViewDirtyRect = rect;
        IntRect visibleAreaWithScrollbars(m_frameView.location(), m_frameView.visibleContentRect(IncludeScrollbars).size());
        scrollViewDirtyRect.intersect(visibleAreaWithScrollbars);
        context->translate(m_frameView.x(), m_frameView.y());
        scrollViewDirtyRect.moveBy(-m_frameView.location());
        context->clip(IntRect(IntPoint(), visibleAreaWithScrollbars.size()));

        paintScrollbars(context, scrollViewDirtyRect);
    }

    // Paint the panScroll Icon
    if (m_frameView.drawPanScrollIcon())
        m_frameView.paintPanScrollIcon(context);
}

void FramePainter::paintContents(GraphicsContext* p, const IntRect& rect)
{
    Document* document = m_frameView.frame().document();

#ifndef NDEBUG
    bool fillWithRed;
    if (document->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (m_frameView.frame().owner())
        fillWithRed = false; // Subframe, don't fill with red.
    else if (m_frameView.isTransparent())
        fillWithRed = false; // Transparent, don't fill with red.
    else if (m_frameView.paintBehavior() & PaintBehaviorSelectionOnly)
        fillWithRed = false; // Selections are transparent, don't fill with red.
    else if (m_frameView.nodeToDraw())
        fillWithRed = false; // Element images are transparent, don't fill with red.
    else
        fillWithRed = true;

    if (fillWithRed)
        p->fillRect(rect, Color(0xFF, 0, 0));
#endif

    RenderView* renderView = m_frameView.renderView();
    if (!renderView) {
        WTF_LOG_ERROR("called FramePainter::paint with nil renderer");
        return;
    }

    RELEASE_ASSERT(!m_frameView.needsLayout());
    ASSERT(document->lifecycle().state() >= DocumentLifecycle::CompositingClean);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Paint", "data", InspectorPaintEvent::data(renderView, rect, 0));
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"), "CallStack", "stack", InspectorCallStackEvent::currentCallStack());
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::willPaint(renderView, 0);

    bool isTopLevelPainter = !s_inPaintContents;
    s_inPaintContents = true;

    FontCachePurgePreventer fontCachePurgePreventer;

    PaintBehavior oldPaintBehavior = m_frameView.paintBehavior();

    if (FrameView* parentView = m_frameView.parentFrameView()) {
        if (parentView->paintBehavior() & PaintBehaviorFlattenCompositingLayers)
            m_frameView.setPaintBehavior(m_frameView.paintBehavior() | PaintBehaviorFlattenCompositingLayers);
    }

    if (m_frameView.paintBehavior() == PaintBehaviorNormal)
        document->markers().invalidateRenderedRectsForMarkersInRect(rect);

    if (document->printing())
        m_frameView.setPaintBehavior(m_frameView.paintBehavior() | PaintBehaviorFlattenCompositingLayers);

    ASSERT(!m_frameView.isPainting());
    m_frameView.setIsPainting(true);

    // m_frameView.nodeToDraw() is used to draw only one element (and its descendants)
    RenderObject* renderer = m_frameView.nodeToDraw() ? m_frameView.nodeToDraw()->renderer() : 0;
    RenderLayer* rootLayer = renderView->layer();

#if ENABLE(ASSERT)
    renderView->assertSubtreeIsLaidOut();
    RenderObject::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(*rootLayer->renderer());
#endif

    LayerPainter layerPainter(*rootLayer);

    layerPainter.paint(p, rect, m_frameView.paintBehavior(), renderer);

    if (rootLayer->containsDirtyOverlayScrollbars())
        layerPainter.paintOverlayScrollbars(p, rect, m_frameView.paintBehavior(), renderer);

    m_frameView.setIsPainting(false);

    m_frameView.setPaintBehavior(oldPaintBehavior);
    m_frameView.setLastPaintTime(currentTime());

    // Regions may have changed as a result of the visibility/z-index of element changing.
    if (document->annotatedRegionsDirty())
        m_frameView.updateAnnotatedRegions();

    if (isTopLevelPainter) {
        // Everything that happens after paintContents completions is considered
        // to be part of the next frame.
        m_frameView.setCurrentFrameTimeStamp(currentTime());
        s_inPaintContents = false;
    }

    InspectorInstrumentation::didPaint(renderView, 0, p, rect);
}

void FramePainter::paintScrollbars(GraphicsContext* context, const IntRect& rect)
{
    if (m_frameView.horizontalScrollbar() && !m_frameView.layerForHorizontalScrollbar())
        paintScrollbar(context, m_frameView.horizontalScrollbar(), rect);
    if (m_frameView.verticalScrollbar() && !m_frameView.layerForVerticalScrollbar())
        paintScrollbar(context, m_frameView.verticalScrollbar(), rect);

    if (m_frameView.layerForScrollCorner())
        return;
    paintScrollCorner(context, m_frameView.scrollCornerRect());
}

void FramePainter::paintScrollCorner(GraphicsContext* context, const IntRect& cornerRect)
{
    if (m_frameView.scrollCorner()) {
        bool needsBackground = m_frameView.frame().isMainFrame();
        if (needsBackground)
            context->fillRect(cornerRect, m_frameView.baseBackgroundColor());
        ScrollbarPainter::paintIntoRect(m_frameView.scrollCorner(), context, cornerRect.location(), cornerRect);
        return;
    }

    ScrollbarTheme::theme()->paintScrollCorner(context, cornerRect);
}

void FramePainter::paintScrollbar(GraphicsContext* context, Scrollbar* bar, const IntRect& rect)
{
    bool needsBackground = bar->isCustomScrollbar() && m_frameView.frame().isMainFrame();
    if (needsBackground) {
        IntRect toFill = bar->frameRect();
        toFill.intersect(rect);
        context->fillRect(toFill, m_frameView.baseBackgroundColor());
    }

    bar->paint(context, rect);
}

void FramePainter::paintOverhangAreas(GraphicsContext* context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    if (m_frameView.frame().document()->printing())
        return;

    if (m_frameView.frame().isMainFrame()) {
        if (m_frameView.frame().page()->chrome().client().paintCustomOverhangArea(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect))
            return;
    }

    paintOverhangAreasInternal(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect);
}

void FramePainter::paintOverhangAreasInternal(GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
{
    ScrollbarTheme::theme()->paintOverhangBackground(context, horizontalOverhangRect, verticalOverhangRect, dirtyRect);
    ScrollbarTheme::theme()->paintOverhangShadows(context, m_frameView.scrollOffset(), horizontalOverhangRect, verticalOverhangRect, dirtyRect);
}

void FramePainter::calculateAndPaintOverhangAreas(GraphicsContext* context, const IntRect& dirtyRect)
{
    IntRect horizontalOverhangRect;
    IntRect verticalOverhangRect;
    m_frameView.calculateOverhangAreasForPainting(horizontalOverhangRect, verticalOverhangRect);

    if (dirtyRect.intersects(horizontalOverhangRect) || dirtyRect.intersects(verticalOverhangRect))
        paintOverhangAreas(context, horizontalOverhangRect, verticalOverhangRect, dirtyRect);
}

} // namespace blink
