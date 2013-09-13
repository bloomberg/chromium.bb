/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/page/FrameView.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/animation/DocumentTimeline.h"
#include "core/css/FontFaceSet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/OverflowEvent.h"
#include "core/editing/FrameSelection.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/TextResourceDecoder.h"
#include "core/html/HTMLFrameElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLPlugInImageElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameActionScheduler.h"
#include "core/page/FrameTree.h"
#include "core/page/Settings.h"
#include "core/page/animation/AnimationController.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/ScrollAnimator.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/text/TextStream.h"
#include "core/rendering/LayoutIndicator.h"
#include "core/rendering/RenderCounter.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerBacking.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderPart.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/RenderScrollbarPart.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/svg/RenderSVGRoot.h"
#include "core/svg/SVGDocument.h"
#include "core/svg/SVGSVGElement.h"

#include "wtf/CurrentTime.h"
#include "wtf/TemporaryChange.h"

#include "core/platform/chromium/TraceEvent.h"

namespace WebCore {

using namespace HTMLNames;

double FrameView::s_currentFrameTimeStamp = 0.0;
bool FrameView::s_inPaintContents = false;


// REPAINT_THROTTLING now chooses default values for throttling parameters.
// Should be removed when applications start using runtime configuration.
#if ENABLE(REPAINT_THROTTLING)
// Normal delay
double FrameView::s_normalDeferredRepaintDelay = 0.016;
// Negative value would mean that first few repaints happen without a delay
double FrameView::s_initialDeferredRepaintDelayDuringLoading = 0;
// The delay grows on each repaint to this maximum value
double FrameView::s_maxDeferredRepaintDelayDuringLoading = 2.5;
// On each repaint the delay increses by this amount
double FrameView::s_deferredRepaintDelayIncrementDuringLoading = 0.5;
#else
// FIXME: Repaint throttling could be good to have on all platform.
// The balance between CPU use and repaint frequency will need some tuning for desktop.
// More hooks may be needed to reset the delay on things like GIF and CSS animations.
double FrameView::s_normalDeferredRepaintDelay = 0;
double FrameView::s_initialDeferredRepaintDelayDuringLoading = 0;
double FrameView::s_maxDeferredRepaintDelayDuringLoading = 0;
double FrameView::s_deferredRepaintDelayIncrementDuringLoading = 0;
#endif

// The maximum number of updateWidgets iterations that should be done before returning.
static const unsigned maxUpdateWidgetsIterations = 2;

static RenderLayer::UpdateLayerPositionsFlags updateLayerPositionFlags(RenderLayer* layer, bool isRelayoutingSubtree, bool didFullRepaint)
{
    RenderLayer::UpdateLayerPositionsFlags flags = RenderLayer::defaultFlags;
    if (didFullRepaint) {
        flags &= ~RenderLayer::CheckForRepaint;
        flags |= RenderLayer::NeedsFullRepaintInBacking;
    }
    if (isRelayoutingSubtree && layer->isPaginated())
        flags |= RenderLayer::UpdatePagination;
    return flags;
}

Pagination::Mode paginationModeForRenderStyle(RenderStyle* style)
{
    EOverflow overflow = style->overflowY();
    if (overflow != OPAGEDX && overflow != OPAGEDY)
        return Pagination::Unpaginated;

    bool isHorizontalWritingMode = style->isHorizontalWritingMode();
    TextDirection textDirection = style->direction();
    WritingMode writingMode = style->writingMode();

    // paged-x always corresponds to LeftToRightPaginated or RightToLeftPaginated. If the WritingMode
    // is horizontal, then we use TextDirection to choose between those options. If the WritingMode
    // is vertical, then the direction of the verticality dictates the choice.
    if (overflow == OPAGEDX) {
        if ((isHorizontalWritingMode && textDirection == LTR) || writingMode == LeftToRightWritingMode)
            return Pagination::LeftToRightPaginated;
        return Pagination::RightToLeftPaginated;
    }

    // paged-y always corresponds to TopToBottomPaginated or BottomToTopPaginated. If the WritingMode
    // is horizontal, then the direction of the horizontality dictates the choice. If the WritingMode
    // is vertical, then we use TextDirection to choose between those options.
    if (writingMode == TopToBottomWritingMode || (!isHorizontalWritingMode && textDirection == RTL))
        return Pagination::TopToBottomPaginated;
    return Pagination::BottomToTopPaginated;
}

FrameView::FrameView(Frame* frame)
    : m_frame(frame)
    , m_canHaveScrollbars(true)
    , m_slowRepaintObjectCount(0)
    , m_layoutTimer(this, &FrameView::layoutTimerFired)
    , m_layoutRoot(0)
    , m_inSynchronousPostLayout(false)
    , m_postLayoutTasksTimer(this, &FrameView::postLayoutTimerFired)
    , m_isTransparent(false)
    , m_baseBackgroundColor(Color::white)
    , m_mediaType("screen")
    , m_actionScheduler(adoptPtr(new FrameActionScheduler))
    , m_overflowStatusDirty(true)
    , m_viewportRenderer(0)
    , m_wasScrolledByUser(false)
    , m_inProgrammaticScroll(false)
    , m_safeToPropagateScrollToParent(true)
    , m_deferredRepaintTimer(this, &FrameView::deferredRepaintTimerFired)
    , m_disableRepaints(0)
    , m_isTrackingRepaints(false)
    , m_shouldUpdateWhileOffscreen(true)
    , m_deferSetNeedsLayouts(0)
    , m_setNeedsLayoutWasDeferred(false)
    , m_scrollCorner(0)
    , m_shouldAutoSize(false)
    , m_inAutoSize(false)
    , m_didRunAutosize(false)
    , m_hasSoftwareFilters(false)
    , m_visibleContentScaleFactor(1)
    , m_partialLayout()
{
    ASSERT(m_frame);
    init();

    if (!isMainFrame())
        return;

    ScrollableArea::setVerticalScrollElasticity(ScrollElasticityAllowed);
    ScrollableArea::setHorizontalScrollElasticity(ScrollElasticityAllowed);
}

PassRefPtr<FrameView> FrameView::create(Frame* frame)
{
    RefPtr<FrameView> view = adoptRef(new FrameView(frame));
    view->show();
    return view.release();
}

PassRefPtr<FrameView> FrameView::create(Frame* frame, const IntSize& initialSize)
{
    RefPtr<FrameView> view = adoptRef(new FrameView(frame));
    view->Widget::setFrameRect(IntRect(view->location(), initialSize));
    view->show();
    return view.release();
}

FrameView::~FrameView()
{
    if (m_postLayoutTasksTimer.isActive()) {
        m_postLayoutTasksTimer.stop();
        m_actionScheduler->clear();
    }

    removeFromAXObjectCache();
    resetScrollbars();

    // Custom scrollbars should already be destroyed at this point
    ASSERT(!horizontalScrollbar() || !horizontalScrollbar()->isCustomScrollbar());
    ASSERT(!verticalScrollbar() || !verticalScrollbar()->isCustomScrollbar());

    setHasHorizontalScrollbar(false); // Remove native scrollbars now before we lose the connection to the HostWindow.
    setHasVerticalScrollbar(false);

    ASSERT(!m_scrollCorner);
    ASSERT(m_actionScheduler->isEmpty());

    ASSERT(m_frame);
    ASSERT(m_frame->view() != this || !m_frame->contentRenderer());
    RenderPart* renderer = m_frame->ownerRenderer();
    if (renderer && renderer->widget() == this)
        renderer->setWidget(0);
}

void FrameView::reset()
{
    m_cannotBlitToWindow = false;
    m_isOverlapped = false;
    m_contentIsOpaque = false;
    m_borderX = 30;
    m_borderY = 30;
    m_layoutTimer.stop();
    m_layoutRoot = 0;
    m_delayedLayout = false;
    m_doFullRepaint = true;
    m_layoutSchedulingEnabled = true;
    m_inLayout = false;
    m_doingPreLayoutStyleUpdate = false;
    m_inSynchronousPostLayout = false;
    m_layoutCount = 0;
    m_nestedLayoutCount = 0;
    m_postLayoutTasksTimer.stop();
    m_firstLayout = true;
    m_firstLayoutCallbackPending = false;
    m_wasScrolledByUser = false;
    m_safeToPropagateScrollToParent = true;
    m_lastViewportSize = IntSize();
    m_lastZoomFactor = 1.0f;
    m_deferringRepaints = 0;
    m_repaintCount = 0;
    m_repaintRects.clear();
    m_deferredRepaintDelay = s_initialDeferredRepaintDelayDuringLoading;
    m_deferredRepaintTimer.stop();
    m_isTrackingRepaints = false;
    m_trackedRepaintRects.clear();
    m_lastPaintTime = 0;
    m_paintBehavior = PaintBehaviorNormal;
    m_isPainting = false;
    m_visuallyNonEmptyCharacterCount = 0;
    m_visuallyNonEmptyPixelCount = 0;
    m_isVisuallyNonEmpty = false;
    m_firstVisuallyNonEmptyLayoutCallbackPending = true;
    m_maintainScrollPositionAnchor = 0;
    m_disableRepaints = 0;
    m_partialLayout.reset();
}

void FrameView::removeFromAXObjectCache()
{
    if (AXObjectCache* cache = axObjectCache())
        cache->remove(this);
}

void FrameView::resetScrollbars()
{
    // Reset the document's scrollbars back to our defaults before we yield the floor.
    m_firstLayout = true;
    setScrollbarsSuppressed(true);
    if (m_canHaveScrollbars)
        setScrollbarModes(ScrollbarAuto, ScrollbarAuto);
    else
        setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
    setScrollbarsSuppressed(false);
}

void FrameView::init()
{
    reset();

    m_margins = LayoutSize(-1, -1); // undefined
    m_size = LayoutSize();

    // Propagate the marginwidth/height and scrolling modes to the view.
    Element* ownerElement = m_frame->ownerElement();
    if (ownerElement && (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))) {
        HTMLFrameElementBase* frameElt = toHTMLFrameElementBase(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            setCanHaveScrollbars(false);
        LayoutUnit marginWidth = frameElt->marginWidth();
        LayoutUnit marginHeight = frameElt->marginHeight();
        if (marginWidth != -1)
            setMarginWidth(marginWidth);
        if (marginHeight != -1)
            setMarginHeight(marginHeight);
    }
}

void FrameView::prepareForDetach()
{
    RELEASE_ASSERT(!isInLayout());

    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->cancelAnimations();

    detachCustomScrollbars();
    // When the view is no longer associated with a frame, it needs to be removed from the ax object cache
    // right now, otherwise it won't be able to reach the topDocument()'s axObject cache later.
    removeFromAXObjectCache();

    if (m_frame->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_frame->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }
}

void FrameView::detachCustomScrollbars()
{
    Scrollbar* horizontalBar = horizontalScrollbar();
    if (horizontalBar && horizontalBar->isCustomScrollbar())
        setHasHorizontalScrollbar(false);

    Scrollbar* verticalBar = verticalScrollbar();
    if (verticalBar && verticalBar->isCustomScrollbar())
        setHasVerticalScrollbar(false);

    if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
    }
}

void FrameView::recalculateScrollbarOverlayStyle()
{
    ScrollbarOverlayStyle oldOverlayStyle = scrollbarOverlayStyle();
    ScrollbarOverlayStyle overlayStyle = ScrollbarOverlayStyleDefault;

    Color backgroundColor = documentBackgroundColor();
    if (backgroundColor.isValid()) {
        // Reduce the background color from RGB to a lightness value
        // and determine which scrollbar style to use based on a lightness
        // heuristic.
        double hue, saturation, lightness;
        backgroundColor.getHSL(hue, saturation, lightness);
        if (lightness <= .5)
            overlayStyle = ScrollbarOverlayStyleLight;
    }

    if (oldOverlayStyle != overlayStyle)
        setScrollbarOverlayStyle(overlayStyle);
}

void FrameView::clear()
{
    setCanBlitOnScroll(true);

    reset();

    if (RenderPart* renderer = m_frame->ownerRenderer())
        renderer->viewCleared();

    setScrollbarsSuppressed(true);
}

bool FrameView::didFirstLayout() const
{
    return !m_firstLayout;
}

void FrameView::invalidateRect(const IntRect& rect)
{
    if (!parent()) {
        if (HostWindow* window = hostWindow())
            window->invalidateContentsAndRootView(rect);
        return;
    }

    RenderPart* renderer = m_frame->ownerRenderer();
    if (!renderer)
        return;

    IntRect repaintRect = rect;
    repaintRect.move(renderer->borderLeft() + renderer->paddingLeft(),
                     renderer->borderTop() + renderer->paddingTop());
    renderer->repaintRectangle(repaintRect);
}

void FrameView::setFrameRect(const IntRect& newRect)
{
    IntRect oldRect = frameRect();
    if (newRect == oldRect)
        return;

    // Autosized font sizes depend on the width of the viewing area.
    if (newRect.width() != oldRect.width()) {
        Page* page = m_frame->page();
        if (isMainFrame() && page->settings().textAutosizingEnabled()) {
            for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
                m_frame->document()->textAutosizer()->recalculateMultipliers();
        }
    }

    ScrollView::setFrameRect(newRect);

    updateScrollableAreaSet();

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor()->frameViewDidChangeSize();
    }

    sendResizeEventIfNeeded();
}

bool FrameView::scheduleAnimation()
{
    if (HostWindow* window = hostWindow()) {
        window->scheduleAnimation();
        return true;
    }
    return false;
}

RenderView* FrameView::renderView() const
{
    return frame().contentRenderer();
}

int FrameView::mapFromLayoutToCSSUnits(LayoutUnit value) const
{
    return value / frame().pageZoomFactor();
}

LayoutUnit FrameView::mapFromCSSToLayoutUnits(int value) const
{
    return value * frame().pageZoomFactor();
}

void FrameView::setMarginWidth(LayoutUnit w)
{
    // make it update the rendering area when set
    m_margins.setWidth(w);
}

void FrameView::setMarginHeight(LayoutUnit h)
{
    // make it update the rendering area when set
    m_margins.setHeight(h);
}

void FrameView::setCanHaveScrollbars(bool canHaveScrollbars)
{
    m_canHaveScrollbars = canHaveScrollbars;
    ScrollView::setCanHaveScrollbars(canHaveScrollbars);
}

void FrameView::updateCanHaveScrollbars()
{
    ScrollbarMode hMode;
    ScrollbarMode vMode;
    scrollbarModes(hMode, vMode);
    if (hMode == ScrollbarAlwaysOff && vMode == ScrollbarAlwaysOff)
        setCanHaveScrollbars(false);
    else
        setCanHaveScrollbars(true);
}

PassRefPtr<Scrollbar> FrameView::createScrollbar(ScrollbarOrientation orientation)
{
    if (Settings* settings = m_frame->settings()) {
        if (!settings->allowCustomScrollbarInMainFrame() && isMainFrame())
            return ScrollView::createScrollbar(orientation);
    }

    // FIXME: We need to update the scrollbar dynamically as documents change (or as doc elements and bodies get discovered that have custom styles).
    Document* doc = m_frame->document();

    // Try the <body> element first as a scrollbar source.
    Element* body = doc ? doc->body() : 0;
    if (body && body->renderer() && body->renderer()->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, body);

    // If the <body> didn't have a custom style, then the root element might.
    Element* docElement = doc ? doc->documentElement() : 0;
    if (docElement && docElement->renderer() && docElement->renderer()->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, docElement);

    // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
    RenderPart* frameRenderer = m_frame->ownerRenderer();
    if (frameRenderer && frameRenderer->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, 0, m_frame.get());

    // Nobody set a custom style, so we just use a native scrollbar.
    return ScrollView::createScrollbar(orientation);
}

void FrameView::setContentsSize(const IntSize& size)
{
    if (size == contentsSize())
        return;

    m_deferSetNeedsLayouts++;

    ScrollView::setContentsSize(size);
    ScrollView::contentsResized();

    Page* page = frame().page();
    if (!page)
        return;

    updateScrollableAreaSet();

    page->chrome().contentsSizeChanged(m_frame.get(), size); // Notify only.

    m_deferSetNeedsLayouts--;

    if (!m_deferSetNeedsLayouts)
        m_setNeedsLayoutWasDeferred = false; // FIXME: Find a way to make the deferred layout actually happen.
}

void FrameView::adjustViewSize()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    ASSERT(m_frame->view() == this);

    const IntRect rect = renderView->documentRect();
    const IntSize& size = rect.size();
    ScrollView::setScrollOrigin(IntPoint(-rect.x(), -rect.y()), !m_frame->document()->printing(), size == contentsSize());

    setContentsSize(size);
}

void FrameView::applyOverflowToViewport(RenderObject* o, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.

    EOverflow overflowX = o->style()->overflowX();
    EOverflow overflowY = o->style()->overflowY();

    if (o->isSVGRoot()) {
        // overflow is ignored in stand-alone SVG documents.
        if (!toRenderSVGRoot(o)->isEmbeddedThroughFrameContainingSVGDocument())
            return;
        overflowX = OHIDDEN;
        overflowY = OHIDDEN;
    }

    switch (overflowX) {
        case OHIDDEN:
            hMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            hMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            hMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }

     switch (overflowY) {
        case OHIDDEN:
            vMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            vMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            vMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all. Values of OPAGEDX and OPAGEDY are handled by applyPaginationToViewPort().
            ;
    }

    m_viewportRenderer = o;
}

void FrameView::applyPaginationToViewport()
{
    Document* document = m_frame->document();
    Node* documentElement = document->documentElement();
    RenderObject* documentRenderer = documentElement ? documentElement->renderer() : 0;
    RenderObject* documentOrBodyRenderer = documentRenderer;
    Node* body = document->body();
    if (body && body->renderer()) {
        if (body->hasTagName(bodyTag))
            documentOrBodyRenderer = documentRenderer->style()->overflowX() == OVISIBLE && isHTMLHtmlElement(documentElement) ? body->renderer() : documentRenderer;
    }

    Pagination pagination;

    if (!documentOrBodyRenderer) {
        setPagination(pagination);
        return;
    }

    EOverflow overflowY = documentOrBodyRenderer->style()->overflowY();
    if (overflowY == OPAGEDX || overflowY == OPAGEDY) {
        pagination.mode = WebCore::paginationModeForRenderStyle(documentOrBodyRenderer->style());
        pagination.gap = static_cast<unsigned>(documentOrBodyRenderer->style()->columnGap());
    }

    setPagination(pagination);
}

void FrameView::calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy strategy)
{
    m_viewportRenderer = 0;

    const HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    if (owner && (owner->scrollingMode() == ScrollbarAlwaysOff)) {
        hMode = ScrollbarAlwaysOff;
        vMode = ScrollbarAlwaysOff;
        return;
    }

    if (m_canHaveScrollbars || strategy == RulesFromWebContentOnly) {
        hMode = ScrollbarAuto;
        // Seamless documents begin with heights of 0; we special case that here
        // to correctly render documents that don't need scrollbars.
        IntSize fullVisibleSize = visibleContentRect(IncludeScrollbars).size();
        bool isSeamlessDocument = frame().document() && frame().document()->shouldDisplaySeamlesslyWithParent();
        vMode = (isSeamlessDocument && !fullVisibleSize.height()) ? ScrollbarAlwaysOff : ScrollbarAuto;
    } else {
        hMode = ScrollbarAlwaysOff;
        vMode = ScrollbarAlwaysOff;
    }

    if (!m_layoutRoot) {
        Document* document = m_frame->document();
        Node* documentElement = document->documentElement();
        RenderObject* rootRenderer = documentElement ? documentElement->renderer() : 0;
        Node* body = document->body();
        if (body && body->renderer()) {
            if (body->hasTagName(framesetTag)) {
                vMode = ScrollbarAlwaysOff;
                hMode = ScrollbarAlwaysOff;
            } else if (body->hasTagName(bodyTag)) {
                // It's sufficient to just check the X overflow,
                // since it's illegal to have visible in only one direction.
                RenderObject* o = rootRenderer->style()->overflowX() == OVISIBLE && isHTMLHtmlElement(document->documentElement()) ? body->renderer() : rootRenderer;
                if (o->style())
                    applyOverflowToViewport(o, hMode, vMode);
            }
        } else if (rootRenderer)
            applyOverflowToViewport(rootRenderer, hMode, vMode);
    }
}

void FrameView::updateCompositingLayersAfterStyleChange()
{
    TRACE_EVENT0("webkit", "FrameView::updateCompositingLayersAfterStyleChange");
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    // If we expect to update compositing after an incipient layout, don't do so here.
    if (m_doingPreLayoutStyleUpdate || layoutPending() || renderView->needsLayout())
        return;

    // This call will make sure the cached hasAcceleratedCompositing is updated from the pref
    renderView->compositor()->cacheAcceleratedCompositingFlags();

    // Sometimes we will change a property (for example, z-index) that will not
    // cause a layout, but will require us to update compositing state. We only
    // need to do this if a layout is not already scheduled.
    if (!needsLayout())
        renderView->compositor()->updateCompositingRequirementsState();

    renderView->compositor()->updateCompositingLayers(CompositingUpdateAfterStyleChange);
}

void FrameView::updateCompositingLayersAfterLayout()
{
    TRACE_EVENT0("webkit", "FrameView::updateCompositingLayersAfterLayout");
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    // This call will make sure the cached hasAcceleratedCompositing is updated from the pref
    renderView->compositor()->cacheAcceleratedCompositingFlags();
    renderView->compositor()->updateCompositingRequirementsState();
    renderView->compositor()->updateCompositingLayers(CompositingUpdateAfterLayout);
}

bool FrameView::usesCompositedScrolling() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;
    if (m_frame->settings() && m_frame->settings()->compositedScrollingForFramesEnabled())
        return renderView->compositor()->inForcedCompositingMode();
    return false;
}

GraphicsLayer* FrameView::layerForScrolling() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->scrollLayer();
}

GraphicsLayer* FrameView::layerForHorizontalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->layerForHorizontalScrollbar();
}

GraphicsLayer* FrameView::layerForVerticalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->layerForVerticalScrollbar();
}

GraphicsLayer* FrameView::layerForScrollCorner() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->layerForScrollCorner();
}

#if ENABLE(RUBBER_BANDING)
GraphicsLayer* FrameView::layerForOverhangAreas() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->layerForOverhangAreas();
}
#endif // ENABLE(RUBBER_BANDING)

bool FrameView::hasCompositedContent() const
{
    if (RenderView* renderView = this->renderView())
        return renderView->compositor()->inCompositingMode();
    return false;
}

bool FrameView::isEnclosedInCompositingLayer() const
{
    RenderObject* frameOwnerRenderer = m_frame->ownerRenderer();
    if (frameOwnerRenderer && frameOwnerRenderer->containerForRepaint())
        return true;

    if (FrameView* parentView = parentFrameView())
        return parentView->isEnclosedInCompositingLayer();

    return false;
}

bool FrameView::isSoftwareRenderable() const
{
    RenderView* renderView = this->renderView();
    return !renderView || !renderView->compositor()->has3DContent();
}

RenderObject* FrameView::layoutRoot(bool onlyDuringLayout) const
{
    return onlyDuringLayout && layoutPending() ? 0 : m_layoutRoot;
}

static inline void collectFrameViewChildren(FrameView* frameView, Vector<RefPtr<FrameView> >& frameViews)
{
    const HashSet<RefPtr<Widget> >* viewChildren = frameView->children();
    ASSERT(viewChildren);

    const HashSet<RefPtr<Widget> >::iterator end = viewChildren->end();
    for (HashSet<RefPtr<Widget> >::iterator current = viewChildren->begin(); current != end; ++current) {
        Widget* widget = (*current).get();
        if (widget->isFrameView())
            frameViews.append(toFrameView(widget));
    }
}

inline void FrameView::forceLayoutParentViewIfNeeded()
{
    RenderPart* ownerRenderer = m_frame->ownerRenderer();
    if (!ownerRenderer || !ownerRenderer->frame())
        return;

    RenderBox* contentBox = embeddedContentBox();
    if (!contentBox)
        return;

    RenderSVGRoot* svgRoot = toRenderSVGRoot(contentBox);
    if (svgRoot->everHadLayout() && !svgRoot->needsLayout())
        return;

    // If the embedded SVG document appears the first time, the ownerRenderer has already finished
    // layout without knowing about the existence of the embedded SVG document, because RenderReplaced
    // embeddedContentBox() returns 0, as long as the embedded document isn't loaded yet. Before
    // bothering to lay out the SVG document, mark the ownerRenderer needing layout and ask its
    // FrameView for a layout. After that the RenderEmbeddedObject (ownerRenderer) carries the
    // correct size, which RenderSVGRoot::computeReplacedLogicalWidth/Height rely on, when laying
    // out for the first time, or when the RenderSVGRoot size has changed dynamically (eg. via <script>).
    RefPtr<FrameView> frameView = ownerRenderer->frame()->view();

    // Mark the owner renderer as needing layout.
    ownerRenderer->setNeedsLayoutAndPrefWidthsRecalc();

    // Synchronously enter layout, to layout the view containing the host object/embed/iframe.
    ASSERT(frameView);
    frameView->layout();
}

void FrameView::performPreLayoutTasks()
{
    // Don't schedule more layouts, we're in one.
    TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

    if (!m_nestedLayoutCount && !m_inSynchronousPostLayout && m_postLayoutTasksTimer.isActive() && !frame().document()->shouldDisplaySeamlesslyWithParent()) {
        // This is a new top-level layout. If there are any remaining tasks from the previous layout, finish them now.
        m_inSynchronousPostLayout = true;
        performPostLayoutTasks();
        m_inSynchronousPostLayout = false;
    }

    // Viewport-dependent media queries may cause us to need completely different style information.
    Document* document = m_frame->document();
    if (!document->styleResolverIfExists() || document->styleResolverIfExists()->affectedByViewportChange()) {
        document->styleResolverChanged(RecalcStyleDeferred);
        // FIXME: This instrumentation event is not strictly accurate since cached media query results
        //        do not persist across StyleResolver rebuilds.
        InspectorInstrumentation::mediaQueryResultChanged(document);
    } else {
        document->evaluateMediaQueryList();
    }

    // If there is any pagination to apply, it will affect the RenderView's style, so we should
    // take care of that now.
    applyPaginationToViewport();

    // Always ensure our style info is up-to-date. This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    TemporaryChange<bool> changeDoingPreLayoutStyleUpdate(m_doingPreLayoutStyleUpdate, true);
    document->updateStyleIfNeeded();
}

void FrameView::performLayout(RenderObject* rootForThisLayout, bool inSubtreeLayout)
{
    // performLayout is the actual guts of layout().
    // FIXME: The 300 other lines in layout() probably belong in other helper functions
    // so that a single human could understand what layout() is actually doing.
    {
        bool disableLayoutState = false;
        if (inSubtreeLayout) {
            RenderView* view = rootForThisLayout->view();
            disableLayoutState = view->shouldDisableLayoutStateForSubtree(rootForThisLayout);
            view->pushLayoutState(rootForThisLayout);
        }
        LayoutStateDisabler layoutStateDisabler(disableLayoutState ? rootForThisLayout->view() : 0);

        m_inLayout = true;

        beginDeferredRepaints();
        forceLayoutParentViewIfNeeded();
        renderView()->updateConfiguration();

        // Text Autosizing requires two-pass layout which is incompatible with partial layout.
        // If enabled, only do partial layout for the second layout.
        // FIXME (crbug.com/256657): Do not do two layouts for text autosizing.
        PartialLayoutDisabler partialLayoutDisabler(partialLayout(), m_frame->settings() && m_frame->settings()->textAutosizingEnabled());

        LayoutIndicator layoutIndicator;
        rootForThisLayout->layout();
    }

    bool autosized = frame().document()->textAutosizer()->processSubtree(rootForThisLayout);
    if (autosized && rootForThisLayout->needsLayout()) {
        TRACE_EVENT0("webkit", "2nd layout due to Text Autosizing");
        LayoutIndicator layoutIndicator;
        rootForThisLayout->layout();
    }

    endDeferredRepaints();
    m_inLayout = false;

    if (inSubtreeLayout)
        rootForThisLayout->view()->popLayoutState(rootForThisLayout);
}

void FrameView::scheduleOrPerformPostLayoutTasks()
{
    if (m_postLayoutTasksTimer.isActive()) {
        m_actionScheduler->resume();
        return;
    }

    // Partial layouts should not happen with synchronous post layouts.
    ASSERT(!(m_inSynchronousPostLayout && partialLayout().isStopping()));

    if (!m_inSynchronousPostLayout) {
        if (frame().document()->shouldDisplaySeamlesslyWithParent()) {
            if (RenderView* renderView = this->renderView())
                renderView->updateWidgetPositions();
        } else {
            m_inSynchronousPostLayout = true;
            // Calls resumeScheduledEvents()
            performPostLayoutTasks();
            m_inSynchronousPostLayout = false;
        }
    }

    if (!m_postLayoutTasksTimer.isActive() && (needsLayout() || m_inSynchronousPostLayout || frame().document()->shouldDisplaySeamlesslyWithParent())) {
        // If we need layout or are already in a synchronous call to postLayoutTasks(),
        // defer widget updates and event dispatch until after we return. postLayoutTasks()
        // can make us need to update again, and we can get stuck in a nasty cycle unless
        // we call it through the timer here.
        m_postLayoutTasksTimer.startOneShot(0);
        if (!partialLayout().isStopping() && needsLayout()) {
            m_actionScheduler->pause();
            layout();
        }
    }
}

void FrameView::layout(bool allowSubtree)
{
    // We should never layout a Document which is not in a Frame.
    ASSERT(m_frame);
    ASSERT(m_frame->view() == this);
    ASSERT(m_frame->page());

    if (m_inLayout)
        return;

    ASSERT(!partialLayout().isStopping());

    TRACE_EVENT0("webkit", "FrameView::layout");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "Layout");

    // Protect the view from being deleted during layout (in recalcStyle)
    RefPtr<FrameView> protector(this);

    // Every scroll that happens during layout is programmatic.
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);

    m_layoutTimer.stop();
    m_delayedLayout = false;
    m_setNeedsLayoutWasDeferred = false;

    // we shouldn't enter layout() while painting
    ASSERT(!isPainting());
    if (isPainting())
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willLayout(m_frame.get());

    if (!allowSubtree && m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }

    performPreLayoutTasks();

    // If there is only one ref to this view left, then its going to be destroyed as soon as we exit,
    // so there's no point to continuing to layout
    if (protector->hasOneRef())
        return;

    Document* document = m_frame->document();
    bool inSubtreeLayout = m_layoutRoot;
    RenderObject* rootForThisLayout = inSubtreeLayout ? m_layoutRoot : document->renderer();
    if (!rootForThisLayout) {
        // FIXME: Do we need to set m_size here?
        ASSERT_NOT_REACHED();
        return;
    }

    bool isPartialLayout = partialLayout().isPartialLayout();

    FontCachePurgePreventer fontCachePurgePreventer;
    RenderLayer* layer;
    {
        TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

        m_nestedLayoutCount++;

        updateCounters();
        autoSizeIfEnabled();

        ScrollbarMode hMode;
        ScrollbarMode vMode;
        calculateScrollbarModesForLayout(hMode, vMode);

        m_doFullRepaint = !inSubtreeLayout && !isPartialLayout && (m_firstLayout || toRenderView(rootForThisLayout)->document().printing());

        if (!inSubtreeLayout && !isPartialLayout) {
            // Now set our scrollbar state for the layout.
            ScrollbarMode currentHMode = horizontalScrollbarMode();
            ScrollbarMode currentVMode = verticalScrollbarMode();

            if (m_firstLayout || (hMode != currentHMode || vMode != currentVMode)) {
                if (m_firstLayout) {
                    setScrollbarsSuppressed(true);

                    m_firstLayout = false;
                    m_firstLayoutCallbackPending = true;
                    m_lastViewportSize = layoutSize(IncludeScrollbars);
                    m_lastZoomFactor = rootForThisLayout->style()->zoom();

                    // Set the initial vMode to AlwaysOn if we're auto.
                    if (vMode == ScrollbarAuto)
                        setVerticalScrollbarMode(ScrollbarAlwaysOn); // This causes a vertical scrollbar to appear.
                    // Set the initial hMode to AlwaysOff if we're auto.
                    if (hMode == ScrollbarAuto)
                        setHorizontalScrollbarMode(ScrollbarAlwaysOff); // This causes a horizontal scrollbar to disappear.

                    setScrollbarModes(hMode, vMode);
                    setScrollbarsSuppressed(false, true);
                } else
                    setScrollbarModes(hMode, vMode);
            }

            LayoutSize oldSize = m_size;

            m_size = LayoutSize(layoutWidth(), layoutHeight());

            if (oldSize != m_size) {
                m_doFullRepaint = true;
                if (!m_firstLayout) {
                    RenderBox* rootRenderer = document->documentElement() ? document->documentElement()->renderBox() : 0;
                    RenderBox* bodyRenderer = rootRenderer && document->body() ? document->body()->renderBox() : 0;
                    if (bodyRenderer && bodyRenderer->stretchesToViewport())
                        bodyRenderer->setChildNeedsLayout();
                    else if (rootRenderer && rootRenderer->stretchesToViewport())
                        rootRenderer->setChildNeedsLayout();
                }
            }
        }

        layer = rootForThisLayout->enclosingLayer();

        m_actionScheduler->pause();

        performLayout(rootForThisLayout, inSubtreeLayout);
        m_layoutRoot = 0;
    } // Reset m_layoutSchedulingEnabled to its previous value.

    bool neededFullRepaint = m_doFullRepaint;

    if (!inSubtreeLayout && !isPartialLayout && !toRenderView(rootForThisLayout)->document().printing())
        adjustViewSize();

    m_doFullRepaint = neededFullRepaint;

    // Now update the positions of all layers.
    beginDeferredRepaints();
    if (m_doFullRepaint) {
        // FIXME: This isn't really right, since the RenderView doesn't fully encompass
        // the visibleContentRect(). It just happens to work out most of the time,
        // since first layouts and printing don't have you scrolled anywhere.
        rootForThisLayout->view()->repaint();
    }

    layer->updateLayerPositionsAfterLayout(renderView()->layer(), updateLayerPositionFlags(layer, inSubtreeLayout, m_doFullRepaint));

    endDeferredRepaints();

    updateCompositingLayersAfterLayout();

    m_layoutCount++;

    if (AXObjectCache* cache = rootForThisLayout->document().existingAXObjectCache())
        cache->postNotification(rootForThisLayout, AXObjectCache::AXLayoutComplete, true);
    updateAnnotatedRegions();

    ASSERT(partialLayout().isStopping() || !rootForThisLayout->needsLayout());

    updateCanBlitOnScrollRecursively();

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(layoutWidth() < contentsWidth(), layoutHeight() < contentsHeight());

    // Resume scheduled events (FrameActionScheduler m_actionScheduler)
    // and ensure post layout tasks are executed or scheduled to be.
    scheduleOrPerformPostLayoutTasks();

    InspectorInstrumentation::didLayout(cookie, rootForThisLayout);

    m_nestedLayoutCount--;
    if (m_nestedLayoutCount)
        return;

    if (partialLayout().isStopping())
        return;

#ifndef NDEBUG
    // Post-layout assert that nobody was re-marked as needing layout during layout.
    document->renderer()->assertSubtreeIsLaidOut();
#endif

    // FIXME: It should be not possible to remove the FrameView from the frame/page during layout
    // however m_inLayout is not set for most of this function, so none of our RELEASE_ASSERTS
    // in Frame/Page will fire. One of the post-layout tasks is disconnecting the Frame from
    // the page in fast/frames/crash-remove-iframe-during-object-beforeload-2.html
    // necessitating this check here.
    // ASSERT(frame()->page());
    if (frame().page())
        frame().page()->chrome().client().layoutUpdated(m_frame.get());
}

RenderBox* FrameView::embeddedContentBox() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;

    RenderObject* firstChild = renderView->firstChild();
    if (!firstChild || !firstChild->isBox())
        return 0;

    // Curently only embedded SVG documents participate in the size-negotiation logic.
    if (firstChild->isSVGRoot())
        return toRenderBox(firstChild);

    return 0;
}

void FrameView::addWidgetToUpdate(RenderObject* object)
{
    if (!m_widgetUpdateSet)
        m_widgetUpdateSet = adoptPtr(new RenderObjectSet);

    // Tell the DOM element that it needs a widget update.
    Node* node = object->node();
    if (node->hasTagName(objectTag) || node->hasTagName(embedTag)) {
        HTMLPlugInImageElement* pluginElement = toHTMLPlugInImageElement(node);
        pluginElement->setNeedsWidgetUpdate(true);
    }

    m_widgetUpdateSet->add(object);
}

void FrameView::removeWidgetToUpdate(RenderObject* object)
{
    if (!m_widgetUpdateSet)
        return;

    m_widgetUpdateSet->remove(object);
}

void FrameView::setMediaType(const AtomicString& mediaType)
{
    m_mediaType = mediaType;
}

AtomicString FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType;
    InspectorInstrumentation::applyEmulatedMedia(m_frame.get(), &overrideType);
    if (!overrideType.isNull())
        return overrideType;
    return m_mediaType;
}

void FrameView::adjustMediaTypeForPrinting(bool printing)
{
    if (printing) {
        if (m_mediaTypeWhenNotPrinting.isNull())
            m_mediaTypeWhenNotPrinting = mediaType();
            setMediaType("print");
    } else {
        if (!m_mediaTypeWhenNotPrinting.isNull())
            setMediaType(m_mediaTypeWhenNotPrinting);
        m_mediaTypeWhenNotPrinting = nullAtom;
    }
}

bool FrameView::useSlowRepaints(bool considerOverlap) const
{
    bool mustBeSlow = m_slowRepaintObjectCount > 0;

    if (contentsInCompositedLayer())
        return mustBeSlow;

    // The chromium compositor does not support scrolling a non-composited frame within a composited page through
    // the fast scrolling path, so force slow scrolling in that case.
    if (m_frame->ownerElement() && !hasCompositedContent() && m_frame->page() && m_frame->page()->mainFrame()->view()->hasCompositedContent())
        return true;

    bool isOverlapped = m_isOverlapped && considerOverlap;

    if (mustBeSlow || m_cannotBlitToWindow || isOverlapped || !m_contentIsOpaque)
        return true;

    if (FrameView* parentView = parentFrameView())
        return parentView->useSlowRepaints(considerOverlap);

    return false;
}

bool FrameView::useSlowRepaintsIfNotOverlapped() const
{
    return useSlowRepaints(false);
}

void FrameView::updateCanBlitOnScrollRecursively()
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
        if (FrameView* view = frame->view())
            view->setCanBlitOnScroll(!view->useSlowRepaints());
    }
}

bool FrameView::contentsInCompositedLayer() const
{
    RenderView* renderView = this->renderView();
    if (renderView && renderView->isComposited()) {
        GraphicsLayer* layer = renderView->layer()->backing()->graphicsLayer();
        if (layer && layer->drawsContent())
            return true;
    }

    return false;
}

void FrameView::setCannotBlitToWindow()
{
    m_cannotBlitToWindow = true;
    updateCanBlitOnScrollRecursively();
}

void FrameView::addSlowRepaintObject()
{
    if (!m_slowRepaintObjectCount++) {
        updateCanBlitOnScrollRecursively();

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
        }
    }
}

void FrameView::removeSlowRepaintObject()
{
    ASSERT(m_slowRepaintObjectCount > 0);
    m_slowRepaintObjectCount--;
    if (!m_slowRepaintObjectCount) {
        updateCanBlitOnScrollRecursively();

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
        }
    }
}

void FrameView::addViewportConstrainedObject(RenderObject* object)
{
    if (!m_viewportConstrainedObjects)
        m_viewportConstrainedObjects = adoptPtr(new ViewportConstrainedObjectSet);

    if (!m_viewportConstrainedObjects->contains(object)) {
        m_viewportConstrainedObjects->add(object);

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(this);
        }
    }
}

void FrameView::removeViewportConstrainedObject(RenderObject* object)
{
    if (m_viewportConstrainedObjects && m_viewportConstrainedObjects->contains(object)) {
        m_viewportConstrainedObjects->remove(object);
        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(this);
        }

        // FIXME: In addFixedObject() we only call this if there's a platform widget,
        // why isn't the same check being made here?
        updateCanBlitOnScrollRecursively();
    }
}

LayoutRect FrameView::viewportConstrainedVisibleContentRect() const
{
    LayoutRect viewportRect = visibleContentRect();
    // Ignore overhang. No-op when not using rubber banding.
    viewportRect.setLocation(clampScrollPosition(scrollPosition()));
    return viewportRect;
}


IntSize FrameView::scrollOffsetForFixedPosition() const
{
    return toIntSize(clampScrollPosition(scrollPosition()));
}

IntPoint FrameView::lastKnownMousePosition() const
{
    return m_frame->eventHandler()->lastKnownMousePosition();
}

bool FrameView::scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    if (!m_viewportConstrainedObjects || m_viewportConstrainedObjects->isEmpty()) {
        hostWindow()->scroll(scrollDelta, rectToScroll, clipRect);
        return true;
    }

    const bool isCompositedContentLayer = contentsInCompositedLayer();

    // Get the rects of the fixed objects visible in the rectToScroll
    Region regionToUpdate;
    ViewportConstrainedObjectSet::const_iterator end = m_viewportConstrainedObjects->end();
    for (ViewportConstrainedObjectSet::const_iterator it = m_viewportConstrainedObjects->begin(); it != end; ++it) {
        RenderObject* renderer = *it;
        if (!renderer->style()->hasViewportConstrainedPosition())
            continue;

        if (renderer->isComposited())
            continue;

        // Fixed items should always have layers.
        ASSERT(renderer->hasLayer());
        RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();

        if (layer->viewportConstrainedNotCompositedReason() == RenderLayer::NotCompositedForBoundsOutOfView
            || layer->viewportConstrainedNotCompositedReason() == RenderLayer::NotCompositedForNoVisibleContent) {
            // Don't invalidate for invisible fixed layers.
            continue;
        }

        if (layer->hasAncestorWithFilterOutsets()) {
            // If the fixed layer has a blur/drop-shadow filter applied on at least one of its parents, we cannot
            // scroll using the fast path, otherwise the outsets of the filter will be moved around the page.
            return false;
        }

        IntRect updateRect = pixelSnappedIntRect(layer->repaintRectIncludingNonCompositingDescendants());

        RenderLayer* enclosingCompositingLayer = layer->enclosingCompositingLayer(false);
        if (enclosingCompositingLayer && !enclosingCompositingLayer->renderer()->isRenderView()) {
            // If the fixed-position layer is contained by a composited layer that is not its containing block,
            // then we have to invlidate that enclosing layer, not the RenderView.
            updateRect.moveBy(scrollPosition());
            IntRect previousRect = updateRect;
            previousRect.move(scrollDelta);
            updateRect.unite(previousRect);
            enclosingCompositingLayer->setBackingNeedsRepaintInRect(updateRect);
        } else {
            // Coalesce the repaints that will be issued to the renderView.
            updateRect = contentsToRootView(updateRect);
            if (!isCompositedContentLayer && clipsRepaints())
                updateRect.intersect(rectToScroll);
            if (!updateRect.isEmpty())
                regionToUpdate.unite(updateRect);
        }
    }

    // 1) scroll
    hostWindow()->scroll(scrollDelta, rectToScroll, clipRect);

    // 2) update the area of fixed objects that has been invalidated
    Vector<IntRect> subRectsToUpdate = regionToUpdate.rects();
    size_t viewportConstrainedObjectsCount = subRectsToUpdate.size();
    for (size_t i = 0; i < viewportConstrainedObjectsCount; ++i) {
        IntRect updateRect = subRectsToUpdate[i];
        IntRect scrolledRect = updateRect;
        scrolledRect.move(scrollDelta);
        updateRect.unite(scrolledRect);
        if (isCompositedContentLayer) {
            updateRect = rootViewToContents(updateRect);
            ASSERT(renderView());
            renderView()->layer()->setBackingNeedsRepaintInRect(updateRect);
            continue;
        }
        if (clipsRepaints())
            updateRect.intersect(rectToScroll);
        hostWindow()->invalidateContentsAndRootView(updateRect);
    }

    return true;
}

void FrameView::scrollContentsSlowPath(const IntRect& updateRect)
{
    if (contentsInCompositedLayer()) {
        IntRect updateRect = visibleContentRect();
        ASSERT(renderView());
        renderView()->layer()->setBackingNeedsRepaintInRect(updateRect);
    }
    if (RenderPart* frameRenderer = m_frame->ownerRenderer()) {
        if (isEnclosedInCompositingLayer()) {
            LayoutRect rect(frameRenderer->borderLeft() + frameRenderer->paddingLeft(),
                            frameRenderer->borderTop() + frameRenderer->paddingTop(),
                            visibleWidth(), visibleHeight());
            frameRenderer->repaintRectangle(rect);
            return;
        }
    }

    ScrollView::scrollContentsSlowPath(updateRect);
}

// Note that this gets called at painting time.
void FrameView::setIsOverlapped(bool isOverlapped)
{
    if (isOverlapped == m_isOverlapped)
        return;

    m_isOverlapped = isOverlapped;
    updateCanBlitOnScrollRecursively();
}

bool FrameView::isOverlappedIncludingAncestors() const
{
    if (isOverlapped())
        return true;

    if (FrameView* parentView = parentFrameView()) {
        if (parentView->isOverlapped())
            return true;
    }

    return false;
}

void FrameView::setContentIsOpaque(bool contentIsOpaque)
{
    if (contentIsOpaque == m_contentIsOpaque)
        return;

    m_contentIsOpaque = contentIsOpaque;
    updateCanBlitOnScrollRecursively();
}

void FrameView::restoreScrollbar()
{
    setScrollbarsSuppressed(false);
}

bool FrameView::scrollToFragment(const KURL& url)
{
    // If our URL has no ref, then we have no place we need to jump to.
    // OTOH If CSS target was set previously, we want to set it to 0, recalc
    // and possibly repaint because :target pseudo class may have been
    // set (see bug 11321).
    if (!url.hasFragmentIdentifier() && !m_frame->document()->cssTarget())
        return false;

    String fragmentIdentifier = url.fragmentIdentifier();
    if (scrollToAnchor(fragmentIdentifier))
        return true;

    // Try again after decoding the ref, based on the document's encoding.
    if (m_frame->document()->encoding().isValid())
        return scrollToAnchor(decodeURLEscapeSequences(fragmentIdentifier, m_frame->document()->encoding()));

    return false;
}

bool FrameView::scrollToAnchor(const String& name)
{
    ASSERT(m_frame->document());

    if (!m_frame->document()->haveStylesheetsLoaded()) {
        m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(true);
        return false;
    }

    m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(false);

    Element* anchorNode = m_frame->document()->findAnchor(name);

    // Setting to null will clear the current target.
    m_frame->document()->setCSSTarget(anchorNode);

    if (m_frame->document()->isSVGDocument()) {
        if (SVGSVGElement* svg = toSVGDocument(m_frame->document())->rootElement()) {
            svg->setupInitialView(name, anchorNode);
            if (!anchorNode)
                return true;
        }
    }

    // Implement the rule that "" and "top" both mean top of page as in other browsers.
    if (!anchorNode && !(name.isEmpty() || equalIgnoringCase(name, "top")))
        return false;

    maintainScrollPositionAtAnchor(anchorNode ? static_cast<Node*>(anchorNode) : m_frame->document());

    // If the anchor accepts keyboard focus, move focus there to aid users relying on keyboard navigation.
    if (anchorNode && anchorNode->isFocusable())
        m_frame->document()->setFocusedElement(anchorNode);

    return true;
}

void FrameView::maintainScrollPositionAtAnchor(Node* anchorNode)
{
    m_maintainScrollPositionAnchor = anchorNode;
    if (!m_maintainScrollPositionAnchor)
        return;

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    m_frame->document()->updateStyleIfNeeded();
    // Only do a layout if changes have occurred that make it necessary.
    RenderView* renderView = this->renderView();
    if (renderView && renderView->needsLayout())
        layout();
    else
        scrollToAnchor();
}

void FrameView::scrollElementToRect(Element* element, const IntRect& rect)
{
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    LayoutRect bounds = element->boundingBox();
    int centeringOffsetX = (rect.width() - bounds.width()) / 2;
    int centeringOffsetY = (rect.height() - bounds.height()) / 2;
    setScrollPosition(IntPoint(bounds.x() - centeringOffsetX - rect.x(), bounds.y() - centeringOffsetY - rect.y()));
}

void FrameView::setScrollPosition(const IntPoint& scrollPoint)
{
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);
    m_maintainScrollPositionAnchor = 0;

    IntPoint newScrollPosition = adjustScrollPositionWithinRange(scrollPoint);

    if (newScrollPosition == scrollPosition())
        return;

    Page* page = m_frame->page();
    if (page && RuntimeEnabledFeatures::programmaticScrollNotificationsEnabled())
        page->chrome().client().didProgrammaticallyScroll(m_frame.get(), newScrollPosition);

    if (requestScrollPositionUpdate(newScrollPosition))
        return;

    ScrollView::setScrollPosition(newScrollPosition);
}

void FrameView::setScrollPositionNonProgrammatically(const IntPoint& scrollPoint)
{
    IntPoint newScrollPosition = adjustScrollPositionWithinRange(scrollPoint);

    if (newScrollPosition == scrollPosition())
        return;

    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, false);
    notifyScrollPositionChanged(newScrollPosition);
}

void FrameView::setViewportConstrainedObjectsNeedLayout()
{
    if (!hasViewportConstrainedObjects())
        return;

    ViewportConstrainedObjectSet::const_iterator end = m_viewportConstrainedObjects->end();
    for (ViewportConstrainedObjectSet::const_iterator it = m_viewportConstrainedObjects->begin(); it != end; ++it) {
        RenderObject* renderer = *it;
        renderer->setNeedsLayout();
    }
}


void FrameView::scrollPositionChanged()
{
    m_frame->eventHandler()->sendScrollEvent();
    m_frame->eventHandler()->dispatchFakeMouseMoveEventSoon();

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor()->frameViewDidScroll();
    }
}

void FrameView::repaintFixedElementsAfterScrolling()
{
    // For fixed position elements, update widget positions and compositing layers after scrolling,
    // but only if we're not inside of layout.
    if (!m_nestedLayoutCount && hasViewportConstrainedObjects()) {
        if (RenderView* renderView = this->renderView()) {
            renderView->updateWidgetPositions();
            renderView->layer()->updateLayerPositionsAfterDocumentScroll();
        }
    }
}

void FrameView::updateFixedElementsAfterScrolling()
{
    if (m_nestedLayoutCount <= 1 && hasViewportConstrainedObjects()) {
        if (RenderView* renderView = this->renderView())
            renderView->compositor()->updateCompositingLayers(CompositingUpdateOnScroll);
    }
}

bool FrameView::shouldRubberBandInDirection(ScrollDirection direction) const
{
    Page* page = frame().page();
    if (!page)
        return ScrollView::shouldRubberBandInDirection(direction);
    return page->chrome().client().shouldRubberBandInDirection(direction);
}

bool FrameView::isRubberBandInProgress() const
{
    if (scrollbarsSuppressed())
        return false;

    // If the main thread updates the scroll position for this FrameView, we should return
    // ScrollAnimator::isRubberBandInProgress().
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();

    return false;
}

bool FrameView::requestScrollPositionUpdate(const IntPoint& position)
{
    return false;
}

HostWindow* FrameView::hostWindow() const
{
    Page* page = frame().page();
    if (!page)
        return 0;
    return &page->chrome();
}

const unsigned cRepaintRectUnionThreshold = 25;

void FrameView::repaintContentRectangle(const IntRect& r)
{
    ASSERT(!m_frame->ownerElement());

    if (m_isTrackingRepaints) {
        IntRect repaintRect = r;
        repaintRect.move(-scrollOffset());
        m_trackedRepaintRects.append(repaintRect);
    }

    double delay = m_deferringRepaints ? 0 : adjustedDeferredRepaintDelay();
    if (m_deferringRepaints || m_deferredRepaintTimer.isActive() || delay) {
        IntRect paintRect = r;
        if (clipsRepaints() && !paintsEntireContents())
            paintRect.intersect(visibleContentRect());
        if (paintRect.isEmpty())
            return;
        if (m_repaintCount == cRepaintRectUnionThreshold) {
            IntRect unionedRect;
            for (unsigned i = 0; i < cRepaintRectUnionThreshold; ++i)
                unionedRect.unite(pixelSnappedIntRect(m_repaintRects[i]));
            m_repaintRects.clear();
            m_repaintRects.append(unionedRect);
        }
        if (m_repaintCount < cRepaintRectUnionThreshold)
            m_repaintRects.append(paintRect);
        else
            m_repaintRects[0].unite(paintRect);
        m_repaintCount++;

        if (!m_deferringRepaints)
            startDeferredRepaintTimer(delay);

        return;
    }

    if (!shouldUpdate())
        return;

    ScrollView::repaintContentRectangle(r);
}

void FrameView::contentsResized()
{
    ScrollView::contentsResized();
    setNeedsLayout();
}

void FrameView::visibleContentsResized()
{
    // We check to make sure the view is attached to a frame() as this method can
    // be triggered before the view is attached by Frame::createView(...) setting
    // various values such as setScrollBarModes(...) for example.  An ASSERT is
    // triggered when a view is layout before being attached to a frame().
    if (!frame().view())
        return;

    if (!useFixedLayout() && needsLayout())
        layout();

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor()->frameViewDidChangeSize();
    }
}

void FrameView::beginDeferredRepaints()
{
    Page* page = m_frame->page();
    ASSERT(page);

    if (!isMainFrame()) {
        page->mainFrame()->view()->beginDeferredRepaints();
        return;
    }

    m_deferringRepaints++;
}

void FrameView::endDeferredRepaints()
{
    Page* page = m_frame->page();
    ASSERT(page);

    if (!isMainFrame()) {
        page->mainFrame()->view()->endDeferredRepaints();
        return;
    }

    ASSERT(m_deferringRepaints > 0);

    if (--m_deferringRepaints)
        return;

    if (m_deferredRepaintTimer.isActive())
        return;

    if (double delay = adjustedDeferredRepaintDelay()) {
        startDeferredRepaintTimer(delay);
        return;
    }

    doDeferredRepaints();
}

void FrameView::startDeferredRepaintTimer(double delay)
{
    if (m_deferredRepaintTimer.isActive())
        return;

    if (m_disableRepaints)
        return;

    m_deferredRepaintTimer.startOneShot(delay);
}

void FrameView::handleLoadCompleted()
{
    // Once loading has completed, allow autoSize one last opportunity to
    // reduce the size of the frame.
    autoSizeIfEnabled();
    if (shouldUseLoadTimeDeferredRepaintDelay())
        return;
    m_deferredRepaintDelay = s_normalDeferredRepaintDelay;
    flushDeferredRepaints();
}

void FrameView::flushDeferredRepaints()
{
    if (!m_deferredRepaintTimer.isActive())
        return;
    m_deferredRepaintTimer.stop();
    doDeferredRepaints();
}

void FrameView::doDeferredRepaints()
{
    if (m_disableRepaints)
        return;

    ASSERT(!m_deferringRepaints);
    if (!shouldUpdate()) {
        m_repaintRects.clear();
        m_repaintCount = 0;
        return;
    }
    unsigned size = m_repaintRects.size();
    for (unsigned i = 0; i < size; i++) {
        ScrollView::repaintContentRectangle(pixelSnappedIntRect(m_repaintRects[i]));
    }
    m_repaintRects.clear();
    m_repaintCount = 0;

    updateDeferredRepaintDelayAfterRepaint();
}

bool FrameView::shouldUseLoadTimeDeferredRepaintDelay() const
{
    // Don't defer after the initial load of the page has been completed.
    if (m_frame->tree()->top()->document()->loadEventFinished())
        return false;
    Document* document = m_frame->document();
    if (!document)
        return false;
    if (document->parsing())
        return true;
    if (document->fetcher()->requestCount())
        return true;
    return false;
}

void FrameView::updateDeferredRepaintDelayAfterRepaint()
{
    if (!shouldUseLoadTimeDeferredRepaintDelay()) {
        m_deferredRepaintDelay = s_normalDeferredRepaintDelay;
        return;
    }
    double incrementedRepaintDelay = m_deferredRepaintDelay + s_deferredRepaintDelayIncrementDuringLoading;
    m_deferredRepaintDelay = std::min(incrementedRepaintDelay, s_maxDeferredRepaintDelayDuringLoading);
}

void FrameView::resetDeferredRepaintDelay()
{
    m_deferredRepaintDelay = 0;
    if (m_deferredRepaintTimer.isActive()) {
        m_deferredRepaintTimer.stop();
        if (!m_deferringRepaints)
            doDeferredRepaints();
    }
}

double FrameView::adjustedDeferredRepaintDelay() const
{
    ASSERT(!m_deferringRepaints);
    if (!m_deferredRepaintDelay)
        return 0;
    double timeSinceLastPaint = currentTime() - m_lastPaintTime;
    return max(0., m_deferredRepaintDelay - timeSinceLastPaint);
}

void FrameView::deferredRepaintTimerFired(Timer<FrameView>*)
{
    doDeferredRepaints();
}

void FrameView::beginDisableRepaints()
{
    m_disableRepaints++;
}

void FrameView::endDisableRepaints()
{
    ASSERT(m_disableRepaints > 0);
    m_disableRepaints--;
}

void FrameView::layoutTimerFired(Timer<FrameView>*)
{
    layout();
}

void FrameView::scheduleRelayout()
{
    ASSERT(m_frame->view() == this);

    if (m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }
    if (!m_layoutSchedulingEnabled)
        return;
    if (!needsLayout())
        return;
    if (!m_frame->document()->shouldScheduleLayout())
        return;
    InspectorInstrumentation::didInvalidateLayout(m_frame.get());

    // When frame seamless is enabled, the contents of the frame could affect the layout of the parent frames.
    // Also invalidate parent frame starting from the owner element of this frame.
    if (m_frame->ownerRenderer() && m_frame->document()->shouldDisplaySeamlesslyWithParent())
        m_frame->ownerRenderer()->setNeedsLayout();

    int delay = m_frame->document()->minimumLayoutDelay();
    if (m_layoutTimer.isActive() && m_delayedLayout && !delay)
        unscheduleRelayout();
    if (m_layoutTimer.isActive())
        return;

    m_delayedLayout = delay != 0;
    m_layoutTimer.startOneShot(delay * 0.001);
}

static bool isObjectAncestorContainerOf(RenderObject* ancestor, RenderObject* descendant)
{
    for (RenderObject* r = descendant; r; r = r->container()) {
        if (r == ancestor)
            return true;
    }
    return false;
}

void FrameView::scheduleRelayoutOfSubtree(RenderObject* relayoutRoot)
{
    ASSERT(m_frame->view() == this);

    RenderView* renderView = this->renderView();
    if (renderView && renderView->needsLayout()) {
        if (relayoutRoot)
            relayoutRoot->markContainingBlocksForLayout(false);
        return;
    }

    if (layoutPending() || !m_layoutSchedulingEnabled) {
        if (m_layoutRoot != relayoutRoot) {
            if (isObjectAncestorContainerOf(m_layoutRoot, relayoutRoot)) {
                // Keep the current root
                relayoutRoot->markContainingBlocksForLayout(false, m_layoutRoot);
                ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
            } else if (m_layoutRoot && isObjectAncestorContainerOf(relayoutRoot, m_layoutRoot)) {
                // Re-root at relayoutRoot
                m_layoutRoot->markContainingBlocksForLayout(false, relayoutRoot);
                m_layoutRoot = relayoutRoot;
                ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
                InspectorInstrumentation::didInvalidateLayout(m_frame.get());
            } else {
                // Just do a full relayout
                if (m_layoutRoot)
                    m_layoutRoot->markContainingBlocksForLayout(false);
                m_layoutRoot = 0;
                relayoutRoot->markContainingBlocksForLayout(false);
                InspectorInstrumentation::didInvalidateLayout(m_frame.get());
            }
        }
    } else if (m_layoutSchedulingEnabled) {
        int delay = m_frame->document()->minimumLayoutDelay();
        m_layoutRoot = relayoutRoot;
        ASSERT(!m_layoutRoot->container() || !m_layoutRoot->container()->needsLayout());
        InspectorInstrumentation::didInvalidateLayout(m_frame.get());
        m_delayedLayout = delay != 0;
        m_layoutTimer.startOneShot(delay * 0.001);
    }
}

bool FrameView::layoutPending() const
{
    return m_layoutTimer.isActive();
}

bool FrameView::needsLayout() const
{
    // This can return true in cases where the document does not have a body yet.
    // Document::shouldScheduleLayout takes care of preventing us from scheduling
    // layout in that case.

    RenderView* renderView = this->renderView();
    return layoutPending()
        || (renderView && renderView->needsLayout())
        || m_layoutRoot
        || (m_deferSetNeedsLayouts && m_setNeedsLayoutWasDeferred);
}

void FrameView::setNeedsLayout()
{
    if (m_deferSetNeedsLayouts) {
        m_setNeedsLayoutWasDeferred = true;
        return;
    }

    if (RenderView* renderView = this->renderView())
        renderView->setNeedsLayout();
}

void FrameView::unscheduleRelayout()
{
    if (!m_layoutTimer.isActive())
        return;

    m_layoutTimer.stop();
    m_delayedLayout = false;
}

void FrameView::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    for (RefPtr<Frame> frame = m_frame; frame; frame = frame->tree()->traverseNext()) {
        frame->view()->serviceScrollAnimations();
        if (!RuntimeEnabledFeatures::webAnimationsCSSEnabled())
            frame->animation()->serviceAnimations();
        if (RuntimeEnabledFeatures::webAnimationsEnabled())
            frame->document()->timeline()->serviceAnimations(monotonicAnimationStartTime);
    }

    Vector<RefPtr<Document> > documents;
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext())
        documents.append(frame->document());

    for (size_t i = 0; i < documents.size(); ++i)
        documents[i]->serviceScriptedAnimations(monotonicAnimationStartTime);
}

bool FrameView::isTransparent() const
{
    return m_isTransparent;
}

void FrameView::setTransparent(bool isTransparent)
{
    m_isTransparent = isTransparent;
    if (renderView() && renderView()->layer()->backing())
        renderView()->layer()->backing()->updateContentsOpaque();
}

bool FrameView::hasOpaqueBackground() const
{
    return !m_isTransparent && !m_baseBackgroundColor.hasAlpha();
}

Color FrameView::baseBackgroundColor() const
{
    return m_baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(const Color& backgroundColor)
{
    if (!backgroundColor.isValid())
        m_baseBackgroundColor = Color::white;
    else
        m_baseBackgroundColor = backgroundColor;

    if (RenderLayerBacking* backing = renderView() ? renderView()->layer()->backing() : 0) {
        backing->updateContentsOpaque();
        if (backing->graphicsLayer())
            backing->graphicsLayer()->setNeedsDisplay();
    }
    recalculateScrollbarOverlayStyle();
}

void FrameView::updateBackgroundRecursively(const Color& backgroundColor, bool transparent)
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
        if (FrameView* view = frame->view()) {
            view->setTransparent(transparent);
            view->setBaseBackgroundColor(backgroundColor);
        }
    }
}

bool FrameView::shouldUpdateWhileOffscreen() const
{
    return m_shouldUpdateWhileOffscreen;
}

void FrameView::setShouldUpdateWhileOffscreen(bool shouldUpdateWhileOffscreen)
{
    m_shouldUpdateWhileOffscreen = shouldUpdateWhileOffscreen;
}

bool FrameView::shouldUpdate() const
{
    if (isOffscreen() && !shouldUpdateWhileOffscreen())
        return false;
    return true;
}

void FrameView::scheduleEvent(PassRefPtr<Event> event, PassRefPtr<Node> eventTarget)
{
    m_actionScheduler->scheduleEvent(event, eventTarget);
}

void FrameView::pauseScheduledEvents()
{
    m_actionScheduler->pause();
}

void FrameView::resumeScheduledEvents()
{
    m_actionScheduler->resume();
}

void FrameView::scrollToAnchor()
{
    RefPtr<Node> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;

    if (!anchorNode->renderer())
        return;

    LayoutRect rect;
    if (anchorNode != m_frame->document())
        rect = anchorNode->boundingBox();

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    anchorNode->renderer()->scrollRectToVisible(rect, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);

    if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
        cache->handleScrolledToAnchor(anchorNode.get());

    // scrollRectToVisible can call into setScrollPosition(), which resets m_maintainScrollPositionAnchor.
    m_maintainScrollPositionAnchor = anchorNode;
}

void FrameView::updateWidget(RenderObject* object)
{
    ASSERT(!object->node() || object->node()->isElementNode());
    Element* ownerElement = toElement(object->node());
    // The object may have already been destroyed (thus node cleared),
    // but FrameView holds a manual ref, so it won't have been deleted.
    ASSERT(m_widgetUpdateSet->contains(object));
    if (!ownerElement)
        return;

    if (object->isEmbeddedObject()) {
        RenderEmbeddedObject* embeddedObject = static_cast<RenderEmbeddedObject*>(object);
        // No need to update if it's already crashed or known to be missing.
        if (embeddedObject->showsUnavailablePluginIndicator())
            return;

        // FIXME: This could turn into a real virtual dispatch if we defined
        // updateWidget(PluginCreationOption) on HTMLElement.
        if (ownerElement->hasTagName(objectTag) || ownerElement->hasTagName(embedTag) || ownerElement->hasTagName(appletTag)) {
            HTMLPlugInImageElement* pluginElement = toHTMLPlugInImageElement(ownerElement);
            if (pluginElement->needsWidgetUpdate())
                pluginElement->updateWidget(CreateAnyWidgetType);
        } else
            ASSERT_NOT_REACHED();

        // Caution: it's possible the object was destroyed again, since loading a
        // plugin may run any arbitrary JavaScript.
        embeddedObject->updateWidgetPosition();
    }
}

bool FrameView::updateWidgets()
{
    if (m_nestedLayoutCount > 1 || !m_widgetUpdateSet || m_widgetUpdateSet->isEmpty())
        return true;

    size_t size = m_widgetUpdateSet->size();

    Vector<RenderObject*> objects;
    objects.reserveInitialCapacity(size);

    RenderObjectSet::const_iterator end = m_widgetUpdateSet->end();
    for (RenderObjectSet::const_iterator it = m_widgetUpdateSet->begin(); it != end; ++it) {
        RenderObject* object = *it;
        objects.uncheckedAppend(object);
        if (object->isEmbeddedObject()) {
            RenderEmbeddedObject* embeddedObject = static_cast<RenderEmbeddedObject*>(object);
            embeddedObject->ref();
        }
    }

    for (size_t i = 0; i < size; ++i) {
        RenderObject* object = objects[i];
        updateWidget(object);
        m_widgetUpdateSet->remove(object);
    }

    for (size_t i = 0; i < size; ++i) {
        RenderObject* object = objects[i];
        if (object->isEmbeddedObject()) {
            RenderEmbeddedObject* embeddedObject = static_cast<RenderEmbeddedObject*>(object);
            embeddedObject->deref();
        }
    }

    return m_widgetUpdateSet->isEmpty();
}

void FrameView::flushAnyPendingPostLayoutTasks()
{
    if (!m_postLayoutTasksTimer.isActive())
        return;

    performPostLayoutTasks();
}

void FrameView::performPostLayoutTasks()
{
    TRACE_EVENT0("webkit", "FrameView::performPostLayoutTasks");
    // updateWidgets() call below can blow us away from underneath.
    RefPtr<FrameView> protect(this);

    m_postLayoutTasksTimer.stop();

    m_frame->selection().setCaretRectNeedsUpdate();
    m_frame->selection().updateAppearance();

    LayoutMilestones milestonesOfInterest = 0;
    LayoutMilestones milestonesAchieved = 0;
    Page* page = m_frame->page();
    if (page)
        milestonesOfInterest = page->layoutMilestones();

    if (m_nestedLayoutCount <= 1) {
        if (m_firstLayoutCallbackPending) {
            m_firstLayoutCallbackPending = false;
            m_frame->loader()->didFirstLayout();
            if (milestonesOfInterest & DidFirstLayout)
                milestonesAchieved |= DidFirstLayout;
            if (isMainFrame())
                page->startCountingRelevantRepaintedObjects();
        }

        // Ensure that we always send this eventually.
        if (!m_frame->document()->parsing() && m_frame->loader()->stateMachine()->committedFirstRealDocumentLoad())
            m_isVisuallyNonEmpty = true;

        // If the layout was done with pending sheets, we are not in fact visually non-empty yet.
        if (m_isVisuallyNonEmpty && !m_frame->document()->didLayoutWithPendingStylesheets() && m_firstVisuallyNonEmptyLayoutCallbackPending) {
            m_firstVisuallyNonEmptyLayoutCallbackPending = false;
            if (milestonesOfInterest & DidFirstVisuallyNonEmptyLayout)
                milestonesAchieved |= DidFirstVisuallyNonEmptyLayout;
        }
    }

    m_frame->loader()->didLayout(milestonesAchieved);
    m_frame->document()->fonts()->didLayout();

    RenderView* renderView = this->renderView();
    if (renderView)
        renderView->updateWidgetPositions();

    for (unsigned i = 0; i < maxUpdateWidgetsIterations; i++) {
        if (updateWidgets())
            break;
    }

    if (page) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewLayoutUpdated(this);
    }

    scrollToAnchor();

    m_actionScheduler->resume();

    sendResizeEventIfNeeded();
}

void FrameView::sendResizeEventIfNeeded()
{
    ASSERT(m_frame);

    RenderView* renderView = this->renderView();
    if (!renderView || renderView->document().printing())
        return;

    IntSize currentSize = layoutSize(IncludeScrollbars);
    float currentZoomFactor = renderView->style()->zoom();

    bool shouldSendResizeEvent = currentSize != m_lastViewportSize || currentZoomFactor != m_lastZoomFactor;

    m_lastViewportSize = currentSize;
    m_lastZoomFactor = currentZoomFactor;

    if (!shouldSendResizeEvent)
        return;

    m_frame->eventHandler()->sendResizeEvent();

    Page* page = m_frame->page();
    if (isMainFrame())
        InspectorInstrumentation::didResizeMainFrame(page);
}

void FrameView::postLayoutTimerFired(Timer<FrameView>*)
{
    performPostLayoutTasks();
}

void FrameView::updateCounters()
{
    RenderView* view = renderView();
    if (!view->hasRenderCounters())
        return;

    for (RenderObject* renderer = view; renderer; renderer = renderer->nextInPreOrder()) {
        if (!renderer->isCounter())
            continue;

        toRenderCounter(renderer)->updateCounter();
    }
}

void FrameView::autoSizeIfEnabled()
{
    if (!m_shouldAutoSize)
        return;

    if (m_inAutoSize)
        return;

    TemporaryChange<bool> changeInAutoSize(m_inAutoSize, true);

    Document* document = frame().document();
    if (!document)
        return;

    RenderView* documentView = document->renderView();
    Element* documentElement = document->documentElement();
    if (!documentView || !documentElement)
        return;

    RenderBox* documentRenderBox = documentElement->renderBox();
    if (!documentRenderBox)
        return;

    // If this is the first time we run autosize, start from small height and
    // allow it to grow.
    if (!m_didRunAutosize)
        resize(frameRect().width(), m_minAutoSize.height());

    IntSize size = frameRect().size();

    // Do the resizing twice. The first time is basically a rough calculation using the preferred width
    // which may result in a height change during the second iteration.
    for (int i = 0; i < 2; i++) {
        // Update various sizes including contentsSize, scrollHeight, etc.
        document->updateLayoutIgnorePendingStylesheets();
        int width = documentView->minPreferredLogicalWidth();
        int height = documentRenderBox->scrollHeight();
        IntSize newSize(width, height);

        // Check to see if a scrollbar is needed for a given dimension and
        // if so, increase the other dimension to account for the scrollbar.
        // Since the dimensions are only for the view rectangle, once a
        // dimension exceeds the maximum, there is no need to increase it further.
        if (newSize.width() > m_maxAutoSize.width()) {
            RefPtr<Scrollbar> localHorizontalScrollbar = horizontalScrollbar();
            if (!localHorizontalScrollbar)
                localHorizontalScrollbar = createScrollbar(HorizontalScrollbar);
            if (!localHorizontalScrollbar->isOverlayScrollbar())
                newSize.setHeight(newSize.height() + localHorizontalScrollbar->height());

            // Don't bother checking for a vertical scrollbar because the width is at
            // already greater the maximum.
        } else if (newSize.height() > m_maxAutoSize.height()) {
            RefPtr<Scrollbar> localVerticalScrollbar = verticalScrollbar();
            if (!localVerticalScrollbar)
                localVerticalScrollbar = createScrollbar(VerticalScrollbar);
            if (!localVerticalScrollbar->isOverlayScrollbar())
                newSize.setWidth(newSize.width() + localVerticalScrollbar->width());

            // Don't bother checking for a horizontal scrollbar because the height is
            // already greater the maximum.
        }

        // Ensure the size is at least the min bounds.
        newSize = newSize.expandedTo(m_minAutoSize);

        // Bound the dimensions by the max bounds and determine what scrollbars to show.
        ScrollbarMode horizonalScrollbarMode = ScrollbarAlwaysOff;
        if (newSize.width() > m_maxAutoSize.width()) {
            newSize.setWidth(m_maxAutoSize.width());
            horizonalScrollbarMode = ScrollbarAlwaysOn;
        }
        ScrollbarMode verticalScrollbarMode = ScrollbarAlwaysOff;
        if (newSize.height() > m_maxAutoSize.height()) {
            newSize.setHeight(m_maxAutoSize.height());
            verticalScrollbarMode = ScrollbarAlwaysOn;
        }

        if (newSize == size)
            continue;

        // While loading only allow the size to increase (to avoid twitching during intermediate smaller states)
        // unless autoresize has just been turned on or the maximum size is smaller than the current size.
        if (m_didRunAutosize && size.height() <= m_maxAutoSize.height() && size.width() <= m_maxAutoSize.width()
            && !m_frame->document()->loadEventFinished() && (newSize.height() < size.height() || newSize.width() < size.width()))
            break;

        resize(newSize.width(), newSize.height());
        // Force the scrollbar state to avoid the scrollbar code adding them and causing them to be needed. For example,
        // a vertical scrollbar may cause text to wrap and thus increase the height (which is the only reason the scollbar is needed).
        setVerticalScrollbarLock(false);
        setHorizontalScrollbarLock(false);
        setScrollbarModes(horizonalScrollbarMode, verticalScrollbarMode, true, true);
    }
    m_didRunAutosize = true;
}

void FrameView::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    if (!m_viewportRenderer)
        return;

    if (m_overflowStatusDirty) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        m_overflowStatusDirty = false;
        return;
    }

    bool horizontalOverflowChanged = (m_horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (m_verticalOverflow != verticalOverflow);

    if (horizontalOverflowChanged || verticalOverflowChanged) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;

        m_actionScheduler->scheduleEvent(OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow,
            verticalOverflowChanged, verticalOverflow),
            m_viewportRenderer->node());
    }

}

const Pagination& FrameView::pagination() const
{
    if (m_pagination != Pagination())
        return m_pagination;

    if (isMainFrame())
        return m_frame->page()->pagination();

    return m_pagination;
}

void FrameView::setPagination(const Pagination& pagination)
{
    if (m_pagination == pagination)
        return;

    m_pagination = pagination;

    m_frame->document()->styleResolverChanged(RecalcStyleDeferred);
}

IntRect FrameView::windowClipRect(bool clipToContents) const
{
    ASSERT(m_frame->view() == this);

    if (paintsEntireContents())
        return IntRect(IntPoint(), contentsSize());

    // Set our clip rect to be our contents.
    IntRect clipRect = contentsToWindow(visibleContentRect(clipToContents ? ExcludeScrollbars : IncludeScrollbars));
    if (!m_frame->ownerElement())
        return clipRect;

    // Take our owner element and get its clip rect.
    HTMLFrameOwnerElement* ownerElement = m_frame->ownerElement();
    FrameView* parentView = ownerElement->document().view();
    if (parentView)
        clipRect.intersect(parentView->windowClipRectForFrameOwner(ownerElement, true));
    return clipRect;
}

IntRect FrameView::windowClipRectForFrameOwner(const HTMLFrameOwnerElement* ownerElement, bool clipToLayerContents) const
{
    // The renderer can sometimes be null when style="display:none" interacts
    // with external content and plugins.
    if (!ownerElement->renderer())
        return windowClipRect();

    // If we have no layer, just return our window clip rect.
    const RenderLayer* enclosingLayer = ownerElement->renderer()->enclosingLayer();
    if (!enclosingLayer)
        return windowClipRect();

    // Apply the clip from the layer.
    IntRect clipRect;
    if (clipToLayerContents)
        clipRect = pixelSnappedIntRect(enclosingLayer->childrenClipRect());
    else
        clipRect = pixelSnappedIntRect(enclosingLayer->selfClipRect());
    clipRect = contentsToWindow(clipRect);
    return intersection(clipRect, windowClipRect());
}

bool FrameView::isActive() const
{
    Page* page = frame().page();
    return page && page->focusController().isActive();
}

void FrameView::scrollTo(const IntSize& newOffset)
{
    LayoutSize offset = scrollOffset();
    ScrollView::scrollTo(newOffset);
    if (offset != scrollOffset())
        scrollPositionChanged();
    frame().loader()->client()->didChangeScrollOffset();
}

void FrameView::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    // Add in our offset within the FrameView.
    IntRect dirtyRect = rect;
    dirtyRect.moveBy(scrollbar->location());
    invalidateRect(dirtyRect);
}

void FrameView::getTickmarks(Vector<IntRect>& tickmarks) const
{
    tickmarks = frame().document()->markers()->renderedRectsForMarkers(DocumentMarker::TextMatch);
}

IntRect FrameView::windowResizerRect() const
{
    Page* page = frame().page();
    if (!page)
        return IntRect();
    return page->chrome().windowResizerRect();
}

void FrameView::setVisibleContentScaleFactor(float visibleContentScaleFactor)
{
    if (m_visibleContentScaleFactor == visibleContentScaleFactor)
        return;

    m_visibleContentScaleFactor = visibleContentScaleFactor;
    updateScrollbars(scrollOffset());
}

bool FrameView::scrollbarsCanBeActive() const
{
    if (m_frame->view() != this)
        return false;

    return !!m_frame->document();
}

ScrollableArea* FrameView::enclosingScrollableArea() const
{
    // FIXME: Walk up the frame tree and look for a scrollable parent frame or RenderLayer.
    return 0;
}

IntRect FrameView::scrollableAreaBoundingBox() const
{
    RenderPart* ownerRenderer = frame().ownerRenderer();
    if (!ownerRenderer)
        return frameRect();

    return ownerRenderer->absoluteContentQuad().enclosingBoundingBox();
}

bool FrameView::isScrollable()
{
    // Check for:
    // 1) If there an actual overflow.
    // 2) display:none or visibility:hidden set to self or inherited.
    // 3) overflow{-x,-y}: hidden;
    // 4) scrolling: no;

    // Covers #1
    IntSize contentsSize = this->contentsSize();
    IntSize visibleContentSize = visibleContentRect().size();
    if ((contentsSize.height() <= visibleContentSize.height() && contentsSize.width() <= visibleContentSize.width()))
        return false;

    // Covers #2.
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    if (owner && (!owner->renderer() || !owner->renderer()->visibleToHitTesting()))
        return false;

    // Cover #3 and #4.
    ScrollbarMode horizontalMode;
    ScrollbarMode verticalMode;
    calculateScrollbarModesForLayout(horizontalMode, verticalMode, RulesFromWebContentOnly);
    if (horizontalMode == ScrollbarAlwaysOff && verticalMode == ScrollbarAlwaysOff)
        return false;

    return true;
}

void FrameView::updateScrollableAreaSet()
{
    // That ensures that only inner frames are cached.
    FrameView* parentFrameView = this->parentFrameView();
    if (!parentFrameView)
        return;

    if (!isScrollable()) {
        parentFrameView->removeScrollableArea(this);
        return;
    }

    parentFrameView->addScrollableArea(this);
}

bool FrameView::shouldSuspendScrollAnimations() const
{
    return m_frame->loader()->state() != FrameStateComplete;
}

void FrameView::scrollbarStyleChanged(int newStyle, bool forceUpdate)
{
    if (!isMainFrame())
        return;

    if (forceUpdate)
        ScrollView::scrollbarStyleChanged(newStyle, forceUpdate);
}

void FrameView::setAnimatorsAreActive()
{
    Page* page = m_frame->page();
    if (!page)
        return;

    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->setIsActive();

    if (!m_scrollableAreas)
        return;

    for (HashSet<ScrollableArea*>::const_iterator it = m_scrollableAreas->begin(), end = m_scrollableAreas->end(); it != end; ++it) {
        ScrollableArea* scrollableArea = *it;

        ASSERT(scrollableArea->scrollbarsCanBeActive());
        scrollableArea->scrollAnimator()->setIsActive();
    }
}

void FrameView::notifyPageThatContentAreaWillPaint() const
{
    Page* page = m_frame->page();
    if (!page)
        return;

    contentAreaWillPaint();

    if (!m_scrollableAreas)
        return;

    for (HashSet<ScrollableArea*>::const_iterator it = m_scrollableAreas->begin(), end = m_scrollableAreas->end(); it != end; ++it) {
        ScrollableArea* scrollableArea = *it;

        if (!scrollableArea->scrollbarsCanBeActive())
            continue;

        scrollableArea->contentAreaWillPaint();
    }
}

bool FrameView::scrollAnimatorEnabled() const
{
    if (Page* page = m_frame->page())
        return page->settings().scrollAnimatorEnabled();
    return false;
}

void FrameView::updateAnnotatedRegions()
{
    Document* document = m_frame->document();
    if (!document->hasAnnotatedRegions())
        return;
    Vector<AnnotatedRegionValue> newRegions;
    document->renderBox()->collectAnnotatedRegions(newRegions);
    if (newRegions == document->annotatedRegions())
        return;
    document->setAnnotatedRegions(newRegions);
    if (Page* page = m_frame->page())
        page->chrome().client().annotatedRegionsChanged();
}

void FrameView::updateScrollCorner()
{
    RefPtr<RenderStyle> cornerStyle;
    IntRect cornerRect = scrollCornerRect();
    Document* doc = m_frame->document();

    if (doc && !cornerRect.isEmpty()) {
        // Try the <body> element first as a scroll corner source.
        if (Element* body = doc->body()) {
            if (RenderObject* renderer = body->renderer())
                cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), renderer->style());
        }

        if (!cornerStyle) {
            // If the <body> didn't have a custom style, then the root element might.
            if (Element* docElement = doc->documentElement()) {
                if (RenderObject* renderer = docElement->renderer())
                    cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), renderer->style());
            }
        }

        if (!cornerStyle) {
            // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
            if (RenderPart* renderer = m_frame->ownerRenderer())
                cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), renderer->style());
        }
    }

    if (cornerStyle) {
        if (!m_scrollCorner)
            m_scrollCorner = RenderScrollbarPart::createAnonymous(doc);
        m_scrollCorner->setStyle(cornerStyle.release());
        invalidateScrollCorner(cornerRect);
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
    }

    ScrollView::updateScrollCorner();
}

void FrameView::paintScrollCorner(GraphicsContext* context, const IntRect& cornerRect)
{
    if (context->updatingControlTints()) {
        updateScrollCorner();
        return;
    }

    if (m_scrollCorner) {
        bool needsBackgorund = isMainFrame();
        if (needsBackgorund)
            context->fillRect(cornerRect, baseBackgroundColor());
        m_scrollCorner->paintIntoRect(context, cornerRect.location(), cornerRect);
        return;
    }

    ScrollView::paintScrollCorner(context, cornerRect);
}

void FrameView::paintScrollbar(GraphicsContext* context, Scrollbar* bar, const IntRect& rect)
{
    bool needsBackgorund = bar->isCustomScrollbar() && isMainFrame();
    if (needsBackgorund) {
        IntRect toFill = bar->frameRect();
        toFill.intersect(rect);
        context->fillRect(toFill, baseBackgroundColor());
    }

    ScrollView::paintScrollbar(context, bar, rect);
}

Color FrameView::documentBackgroundColor() const
{
    // <https://bugs.webkit.org/show_bug.cgi?id=59540> We blend the background color of
    // the document and the body against the base background color of the frame view.
    // Background images are unfortunately impractical to include.

    // Return invalid Color objects whenever there is insufficient information.
    if (!frame().document())
        return Color();

    Element* htmlElement = frame().document()->documentElement();
    Element* bodyElement = frame().document()->body();

    // Start with invalid colors.
    Color htmlBackgroundColor;
    Color bodyBackgroundColor;
    if (htmlElement && htmlElement->renderer())
        htmlBackgroundColor = htmlElement->renderer()->style()->visitedDependentColor(CSSPropertyBackgroundColor);
    if (bodyElement && bodyElement->renderer())
        bodyBackgroundColor = bodyElement->renderer()->style()->visitedDependentColor(CSSPropertyBackgroundColor);

    if (!bodyBackgroundColor.isValid()) {
        if (!htmlBackgroundColor.isValid())
            return Color();
        return baseBackgroundColor().blend(htmlBackgroundColor);
    }

    if (!htmlBackgroundColor.isValid())
        return baseBackgroundColor().blend(bodyBackgroundColor);

    // We take the aggregate of the base background color
    // the <html> background color, and the <body>
    // background color to find the document color. The
    // addition of the base background color is not
    // technically part of the document background, but it
    // otherwise poses problems when the aggregate is not
    // fully opaque.
    return baseBackgroundColor().blend(htmlBackgroundColor).blend(bodyBackgroundColor);
}

bool FrameView::hasCustomScrollbars() const
{
    const HashSet<RefPtr<Widget> >* viewChildren = children();
    HashSet<RefPtr<Widget> >::const_iterator end = viewChildren->end();
    for (HashSet<RefPtr<Widget> >::const_iterator current = viewChildren->begin(); current != end; ++current) {
        Widget* widget = current->get();
        if (widget->isFrameView()) {
            if (toFrameView(widget)->hasCustomScrollbars())
                return true;
        } else if (widget->isScrollbar()) {
            Scrollbar* scrollbar = static_cast<Scrollbar*>(widget);
            if (scrollbar->isCustomScrollbar())
                return true;
        }
    }

    return false;
}

FrameView* FrameView::parentFrameView() const
{
    if (!parent())
        return 0;

    if (Frame* parentFrame = m_frame->tree()->parent())
        return parentFrame->view();

    return 0;
}

void FrameView::updateControlTints()
{
    // This is called when control tints are changed from aqua/graphite to clear and vice versa.
    // We do a "fake" paint, and when the theme gets a paint call, it can then do an invalidate.
    // This is only done if the theme supports control tinting. It's up to the theme and platform
    // to define when controls get the tint and to call this function when that changes.

    // Optimize the common case where we bring a window to the front while it's still empty.
    if (m_frame->document()->url().isEmpty())
        return;

    if (RenderTheme::theme().supportsControlTints() || hasCustomScrollbars())
        paintControlTints();
}

void FrameView::paintControlTints()
{
    if (needsLayout())
        layout();
    // FIXME: The use of paint seems like overkill: crbug.com/236892
    GraphicsContext context(0); // NULL canvas to get a non-painting context.
    context.setUpdatingControlTints(true);
    paint(&context, frameRect());
}

bool FrameView::wasScrolledByUser() const
{
    return m_wasScrolledByUser;
}

void FrameView::setWasScrolledByUser(bool wasScrolledByUser)
{
    if (m_inProgrammaticScroll)
        return;
    m_maintainScrollPositionAnchor = 0;
    m_wasScrolledByUser = wasScrolledByUser;
}

void FrameView::paintContents(GraphicsContext* p, const IntRect& rect)
{
    Document* document = m_frame->document();

#ifndef NDEBUG
    bool fillWithRed;
    if (document->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (m_frame->ownerElement())
        fillWithRed = false; // Subframe, don't fill with red.
    else if (isTransparent())
        fillWithRed = false; // Transparent, don't fill with red.
    else if (m_paintBehavior & PaintBehaviorSelectionOnly)
        fillWithRed = false; // Selections are transparent, don't fill with red.
    else if (m_nodeToDraw)
        fillWithRed = false; // Element images are transparent, don't fill with red.
    else
        fillWithRed = true;

    if (fillWithRed)
        p->fillRect(rect, Color(0xFF, 0, 0));
#endif

    RenderView* renderView = this->renderView();
    if (!renderView) {
        LOG_ERROR("called FrameView::paint with nil renderer");
        return;
    }

    ASSERT(!needsLayout());
    if (needsLayout())
        return;

    InspectorInstrumentation::willPaint(renderView);

    bool isTopLevelPainter = !s_inPaintContents;
    s_inPaintContents = true;

    FontCachePurgePreventer fontCachePurgePreventer;

    PaintBehavior oldPaintBehavior = m_paintBehavior;

    if (FrameView* parentView = parentFrameView()) {
        if (parentView->paintBehavior() & PaintBehaviorFlattenCompositingLayers)
            m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;
    }

    if (m_paintBehavior == PaintBehaviorNormal)
        document->markers()->invalidateRenderedRectsForMarkersInRect(rect);

    if (document->printing())
        m_paintBehavior |= PaintBehaviorFlattenCompositingLayers;

    ASSERT(!m_isPainting);
    m_isPainting = true;

    // m_nodeToDraw is used to draw only one element (and its descendants)
    RenderObject* eltRenderer = m_nodeToDraw ? m_nodeToDraw->renderer() : 0;
    RenderLayer* rootLayer = renderView->layer();

#ifndef NDEBUG
    renderView->assertSubtreeIsLaidOut();
    RenderObject::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(rootLayer->renderer());
#endif

    rootLayer->paint(p, rect, m_paintBehavior, eltRenderer);

    if (rootLayer->containsDirtyOverlayScrollbars())
        rootLayer->paintOverlayScrollbars(p, rect, m_paintBehavior, eltRenderer);

    m_isPainting = false;

    m_paintBehavior = oldPaintBehavior;
    m_lastPaintTime = currentTime();

    // Regions may have changed as a result of the visibility/z-index of element changing.
    if (document->annotatedRegionsDirty())
        updateAnnotatedRegions();

    if (isTopLevelPainter) {
        // Everythin that happens after paintContents completions is considered
        // to be part of the next frame.
        s_currentFrameTimeStamp = currentTime();
        s_inPaintContents = false;
    }

    InspectorInstrumentation::didPaint(renderView, p, rect);
}

void FrameView::setPaintBehavior(PaintBehavior behavior)
{
    m_paintBehavior = behavior;
}

PaintBehavior FrameView::paintBehavior() const
{
    return m_paintBehavior;
}

bool FrameView::isPainting() const
{
    return m_isPainting;
}

void FrameView::setNodeToDraw(Node* node)
{
    m_nodeToDraw = node;
}

void FrameView::paintOverhangAreas(GraphicsContext* context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    if (context->paintingDisabled())
        return;

    if (m_frame->document()->printing())
        return;

    if (isMainFrame()) {
        if (m_frame->page()->chrome().client().paintCustomOverhangArea(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect))
            return;
    }

    ScrollView::paintOverhangAreas(context, horizontalOverhangArea, verticalOverhangArea, dirtyRect);
}

void FrameView::updateLayoutAndStyleIfNeededRecursive()
{
    // We have to crawl our entire tree looking for any FrameViews that need
    // layout and make sure they are up to date.
    // Mac actually tests for intersection with the dirty region and tries not to
    // update layout for frames that are outside the dirty region.  Not only does this seem
    // pointless (since those frames will have set a zero timer to layout anyway), but
    // it is also incorrect, since if two frames overlap, the first could be excluded from the dirty
    // region but then become included later by the second frame adding rects to the dirty region
    // when it lays out.

    m_frame->document()->updateStyleIfNeeded();

    if (needsLayout())
        layout();

    // Grab a copy of the children() set, as it may be mutated by the following updateLayoutAndStyleIfNeededRecursive
    // calls, as they can potentially re-enter a layout of the parent frame view, which may add/remove scrollbars
    // and thus mutates the children() set.
    Vector<RefPtr<FrameView> > frameViews;
    collectFrameViewChildren(this, frameViews);

    const Vector<RefPtr<FrameView> >::iterator end = frameViews.end();
    for (Vector<RefPtr<FrameView> >::iterator it = frameViews.begin(); it != end; ++it)
        (*it)->updateLayoutAndStyleIfNeededRecursive();

    // updateLayoutAndStyleIfNeededRecursive is called when we need to make sure style and layout are up-to-date before
    // painting, so we need to flush out any deferred repaints too.
    flushDeferredRepaints();

    // When seamless is on, child frame can mark parent frame dirty. In such case, child frame
    // needs to call layout on parent frame recursively.
    // This assert ensures that parent frames are clean, when child frames finished updating layout and style.
    ASSERT(!needsLayout());
}

void FrameView::enableAutoSizeMode(bool enable, const IntSize& minSize, const IntSize& maxSize)
{
    ASSERT(!enable || !minSize.isEmpty());
    ASSERT(minSize.width() <= maxSize.width());
    ASSERT(minSize.height() <= maxSize.height());

    if (m_shouldAutoSize == enable && m_minAutoSize == minSize && m_maxAutoSize == maxSize)
        return;

    m_shouldAutoSize = enable;
    m_minAutoSize = minSize;
    m_maxAutoSize = maxSize;
    m_didRunAutosize = false;

    setNeedsLayout();
    scheduleRelayout();
    if (m_shouldAutoSize)
        return;

    // Since autosize mode forces the scrollbar mode, change them to being auto.
    setVerticalScrollbarLock(false);
    setHorizontalScrollbarLock(false);
    setScrollbarModes(ScrollbarAuto, ScrollbarAuto);
}

void FrameView::forceLayout(bool allowSubtree)
{
    layout(allowSubtree);
}

void FrameView::forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor, AdjustViewSizeOrNot shouldAdjustViewSize)
{
    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    if (RenderView* renderView = this->renderView()) {
        float pageLogicalWidth = renderView->style()->isHorizontalWritingMode() ? pageSize.width() : pageSize.height();
        float pageLogicalHeight = renderView->style()->isHorizontalWritingMode() ? pageSize.height() : pageSize.width();

        LayoutUnit flooredPageLogicalWidth = static_cast<LayoutUnit>(pageLogicalWidth);
        LayoutUnit flooredPageLogicalHeight = static_cast<LayoutUnit>(pageLogicalHeight);
        renderView->setLogicalWidth(flooredPageLogicalWidth);
        renderView->setPageLogicalHeight(flooredPageLogicalHeight);
        renderView->setNeedsLayoutAndPrefWidthsRecalc();
        forceLayout();

        // If we don't fit in the given page width, we'll lay out again. If we don't fit in the
        // page width when shrunk, we will lay out at maximum shrink and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        bool horizontalWritingMode = renderView->style()->isHorizontalWritingMode();
        const LayoutRect& documentRect = renderView->documentRect();
        LayoutUnit docLogicalWidth = horizontalWritingMode ? documentRect.width() : documentRect.height();
        if (docLogicalWidth > pageLogicalWidth) {
            int expectedPageWidth = std::min<float>(documentRect.width(), pageSize.width() * maximumShrinkFactor);
            int expectedPageHeight = std::min<float>(documentRect.height(), pageSize.height() * maximumShrinkFactor);
            FloatSize maxPageSize = m_frame->resizePageRectsKeepingRatio(FloatSize(originalPageSize.width(), originalPageSize.height()), FloatSize(expectedPageWidth, expectedPageHeight));
            pageLogicalWidth = horizontalWritingMode ? maxPageSize.width() : maxPageSize.height();
            pageLogicalHeight = horizontalWritingMode ? maxPageSize.height() : maxPageSize.width();

            flooredPageLogicalWidth = static_cast<LayoutUnit>(pageLogicalWidth);
            flooredPageLogicalHeight = static_cast<LayoutUnit>(pageLogicalHeight);
            renderView->setLogicalWidth(flooredPageLogicalWidth);
            renderView->setPageLogicalHeight(flooredPageLogicalHeight);
            renderView->setNeedsLayoutAndPrefWidthsRecalc();
            forceLayout();

            const LayoutRect& updatedDocumentRect = renderView->documentRect();
            LayoutUnit docLogicalHeight = horizontalWritingMode ? updatedDocumentRect.height() : updatedDocumentRect.width();
            LayoutUnit docLogicalTop = horizontalWritingMode ? updatedDocumentRect.y() : updatedDocumentRect.x();
            LayoutUnit docLogicalRight = horizontalWritingMode ? updatedDocumentRect.maxX() : updatedDocumentRect.maxY();
            LayoutUnit clippedLogicalLeft = 0;
            if (!renderView->style()->isLeftToRightDirection())
                clippedLogicalLeft = docLogicalRight - pageLogicalWidth;
            LayoutRect overflow(clippedLogicalLeft, docLogicalTop, pageLogicalWidth, docLogicalHeight);

            if (!horizontalWritingMode)
                overflow = overflow.transposedRect();
            renderView->clearLayoutOverflow();
            renderView->addLayoutOverflow(overflow); // This is how we clip in case we overflow again.
        }
    }

    if (shouldAdjustViewSize)
        adjustViewSize();
}

IntRect FrameView::convertFromRenderer(const RenderObject* renderer, const IntRect& rendererRect) const
{
    IntRect rect = pixelSnappedIntRect(enclosingLayoutRect(renderer->localToAbsoluteQuad(FloatRect(rendererRect)).boundingBox()));

    // Convert from page ("absolute") to FrameView coordinates.
    rect.moveBy(-scrollPosition());

    return rect;
}

IntRect FrameView::convertToRenderer(const RenderObject* renderer, const IntRect& viewRect) const
{
    IntRect rect = viewRect;

    // Convert from FrameView coords into page ("absolute") coordinates.
    rect.moveBy(scrollPosition());

    // FIXME: we don't have a way to map an absolute rect down to a local quad, so just
    // move the rect for now.
    rect.setLocation(roundedIntPoint(renderer->absoluteToLocal(rect.location(), UseTransforms)));
    return rect;
}

IntPoint FrameView::convertFromRenderer(const RenderObject* renderer, const IntPoint& rendererPoint) const
{
    IntPoint point = roundedIntPoint(renderer->localToAbsolute(rendererPoint, UseTransforms));

    // Convert from page ("absolute") to FrameView coordinates.
    point.moveBy(-scrollPosition());
    return point;
}

IntPoint FrameView::convertToRenderer(const RenderObject* renderer, const IntPoint& viewPoint) const
{
    IntPoint point = viewPoint;

    // Convert from FrameView coords into page ("absolute") coordinates.
    point += IntSize(scrollX(), scrollY());

    return roundedIntPoint(renderer->absoluteToLocal(point, UseTransforms));
}

IntRect FrameView::convertToContainingView(const IntRect& localRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);
            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return localRect;

            IntRect rect(localRect);
            // Add borders and padding??
            rect.move(renderer->borderLeft() + renderer->paddingLeft(),
                      renderer->borderTop() + renderer->paddingTop());
            return parentView->convertFromRenderer(renderer, rect);
        }

        return Widget::convertToContainingView(localRect);
    }

    return localRect;
}

IntRect FrameView::convertFromContainingView(const IntRect& parentRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);

            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return parentRect;

            IntRect rect = parentView->convertToRenderer(renderer, parentRect);
            // Subtract borders and padding
            rect.move(-renderer->borderLeft() - renderer->paddingLeft(),
                      -renderer->borderTop() - renderer->paddingTop());
            return rect;
        }

        return Widget::convertFromContainingView(parentRect);
    }

    return parentRect;
}

IntPoint FrameView::convertToContainingView(const IntPoint& localPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);

            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return localPoint;

            IntPoint point(localPoint);

            // Add borders and padding
            point.move(renderer->borderLeft() + renderer->paddingLeft(),
                       renderer->borderTop() + renderer->paddingTop());
            return parentView->convertFromRenderer(renderer, point);
        }

        return Widget::convertToContainingView(localPoint);
    }

    return localPoint;
}

IntPoint FrameView::convertFromContainingView(const IntPoint& parentPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        if (parentScrollView->isFrameView()) {
            const FrameView* parentView = toFrameView(parentScrollView);

            // Get our renderer in the parent view
            RenderPart* renderer = m_frame->ownerRenderer();
            if (!renderer)
                return parentPoint;

            IntPoint point = parentView->convertToRenderer(renderer, parentPoint);
            // Subtract borders and padding
            point.move(-renderer->borderLeft() - renderer->paddingLeft(),
                       -renderer->borderTop() - renderer->paddingTop());
            return point;
        }

        return Widget::convertFromContainingView(parentPoint);
    }

    return parentPoint;
}

// Normal delay
void FrameView::setRepaintThrottlingDeferredRepaintDelay(double p)
{
    s_normalDeferredRepaintDelay = p;
}

// Negative value would mean that first few repaints happen without a delay
void FrameView::setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(double p)
{
    s_initialDeferredRepaintDelayDuringLoading = p;
}

// The delay grows on each repaint to this maximum value
void FrameView::setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(double p)
{
    s_maxDeferredRepaintDelayDuringLoading = p;
}

// On each repaint the delay increases by this amount
void FrameView::setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(double p)
{
    s_deferredRepaintDelayIncrementDuringLoading = p;
}

void FrameView::setTracksRepaints(bool trackRepaints)
{
    if (trackRepaints == m_isTrackingRepaints)
        return;

    for (Frame* frame = m_frame->tree()->top(); frame; frame = frame->tree()->traverseNext()) {
        if (RenderView* renderView = frame->contentRenderer())
            renderView->compositor()->setTracksRepaints(trackRepaints);
    }

    resetTrackedRepaints();
    m_isTrackingRepaints = trackRepaints;
}

void FrameView::resetTrackedRepaints()
{
    m_trackedRepaintRects.clear();
    if (RenderView* renderView = this->renderView())
        renderView->compositor()->resetTrackedRepaintRects();
}

String FrameView::trackedRepaintRectsAsText() const
{
    TextStream ts;
    if (!m_trackedRepaintRects.isEmpty()) {
        ts << "(repaint rects\n";
        for (size_t i = 0; i < m_trackedRepaintRects.size(); ++i)
            ts << "  (rect " << m_trackedRepaintRects[i].x() << " " << m_trackedRepaintRects[i].y() << " " << m_trackedRepaintRects[i].width() << " " << m_trackedRepaintRects[i].height() << ")\n";
        ts << ")\n";
    }
    return ts.release();
}

void FrameView::addResizerArea(RenderLayer* resizerLayer)
{
    if (!m_resizerAreas)
        m_resizerAreas = adoptPtr(new ResizerAreaSet);
    m_resizerAreas->add(resizerLayer);
}

void FrameView::removeResizerArea(RenderLayer* resizerLayer)
{
    if (!m_resizerAreas)
        return;

    ResizerAreaSet::iterator it = m_resizerAreas->find(resizerLayer);
    if (it != m_resizerAreas->end())
        m_resizerAreas->remove(it);
}

bool FrameView::addScrollableArea(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreas)
        m_scrollableAreas = adoptPtr(new ScrollableAreaSet);
    return m_scrollableAreas->add(scrollableArea).isNewEntry;
}

bool FrameView::removeScrollableArea(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreas)
        return false;

    ScrollableAreaSet::iterator it = m_scrollableAreas->find(scrollableArea);
    if (it == m_scrollableAreas->end())
        return false;

    m_scrollableAreas->remove(it);
    return true;
}

bool FrameView::containsScrollableArea(const ScrollableArea* scrollableArea) const
{
    if (!m_scrollableAreas)
        return false;
    return m_scrollableAreas->contains(const_cast<ScrollableArea*>(scrollableArea));
}

void FrameView::removeChild(Widget* widget)
{
    if (widget->isFrameView())
        removeScrollableArea(toFrameView(widget));

    ScrollView::removeChild(widget);
}

bool FrameView::wheelEvent(const PlatformWheelEvent& wheelEvent)
{
    // Note that to allow for rubber-band over-scroll behavior, even non-scrollable views
    // should handle wheel events.
#if !ENABLE(RUBBER_BANDING)
    if (!isScrollable())
        return false;
#endif

    // We don't allow mouse wheeling to happen in a ScrollView that has had its scrollbars explicitly disabled.
    if (!canHaveScrollbars())
        return false;

    return ScrollableArea::handleWheelEvent(wheelEvent);
}

bool FrameView::isVerticalDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return true;

    return renderView->style()->isHorizontalWritingMode();
}

bool FrameView::isFlippedDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    return renderView->style()->isFlippedBlocksWritingMode();
}

AXObjectCache* FrameView::axObjectCache() const
{
    if (frame().document())
        return frame().document()->existingAXObjectCache();
    return 0;
}

bool FrameView::isMainFrame() const
{
    return m_frame->page() && m_frame->page()->mainFrame() == m_frame;
}

} // namespace WebCore
