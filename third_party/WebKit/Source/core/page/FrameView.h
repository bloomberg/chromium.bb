/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef FrameView_h
#define FrameView_h

#include "core/page/AdjustViewSizeOrNot.h"
#include "core/platform/ScrollView.h"
#include "core/platform/graphics/Color.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/rendering/Pagination.h"
#include "core/rendering/PaintPhase.h"
#include "core/rendering/PartialLayoutState.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Element;
class Event;
class FloatSize;
class Frame;
class FrameActionScheduler;
class KURL;
class Node;
class Page;
class RenderBox;
class RenderEmbeddedObject;
class RenderLayer;
class RenderObject;
class RenderScrollbarPart;
class RenderStyle;

Pagination::Mode paginationModeForRenderStyle(RenderStyle*);

typedef unsigned long long DOMTimeStamp;

class FrameView : public ScrollView {
public:
    friend class RenderView;
    friend class Internals;

    static PassRefPtr<FrameView> create(Frame*);
    static PassRefPtr<FrameView> create(Frame*, const IntSize& initialSize);

    virtual ~FrameView();

    virtual HostWindow* hostWindow() const;

    virtual void invalidateRect(const IntRect&);
    virtual void setFrameRect(const IntRect&);

    virtual bool scheduleAnimation();

    Frame& frame() const { return *m_frame; }

    RenderView* renderView() const;

    int mapFromLayoutToCSSUnits(LayoutUnit) const;
    LayoutUnit mapFromCSSToLayoutUnits(int) const;

    LayoutUnit marginWidth() const { return m_margins.width(); } // -1 means default
    LayoutUnit marginHeight() const { return m_margins.height(); } // -1 means default
    void setMarginWidth(LayoutUnit);
    void setMarginHeight(LayoutUnit);

    virtual void setCanHaveScrollbars(bool);
    void updateCanHaveScrollbars();

    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation);

    virtual void setContentsSize(const IntSize&);

    void layout(bool allowSubtree = true);
    bool didFirstLayout() const;
    void layoutTimerFired(Timer<FrameView>*);
    void scheduleRelayout();
    void scheduleRelayoutOfSubtree(RenderObject*);
    void unscheduleRelayout();
    bool layoutPending() const;
    bool isInLayout() const { return m_inLayout; }

    RenderObject* layoutRoot(bool onlyDuringLayout = false) const;
    void clearLayoutRoot() { m_layoutRoot = 0; }
    int layoutCount() const { return m_layoutCount; }

    bool needsLayout() const;
    void setNeedsLayout();
    void setViewportConstrainedObjectsNeedLayout();

    bool needsFullRepaint() const { return m_doFullRepaint; }

    void serviceScriptedAnimations(double monotonicAnimationStartTime);

    void updateCompositingLayersAfterStyleChange();
    void updateCompositingLayersAfterLayout();

    bool hasCompositedContent() const;
    bool isEnclosedInCompositingLayer() const;

    // Returns true when a paint with the PaintBehaviorFlattenCompositingLayers flag set gives
    // a faithful representation of the content.
    bool isSoftwareRenderable() const;

    void resetScrollbars();
    void prepareForDetach();
    void detachCustomScrollbars();
    virtual void recalculateScrollbarOverlayStyle();

    void clear();

    bool isTransparent() const;
    void setTransparent(bool isTransparent);

    // True if the FrameView is not transparent, and the base background color is opaque.
    bool hasOpaqueBackground() const;

    Color baseBackgroundColor() const;
    void setBaseBackgroundColor(const Color&);
    void updateBackgroundRecursively(const Color&, bool);

    bool shouldUpdateWhileOffscreen() const;
    void setShouldUpdateWhileOffscreen(bool);
    bool shouldUpdate() const;

    void adjustViewSize();

    virtual IntRect windowClipRect(bool clipToContents = true) const;
    IntRect windowClipRectForFrameOwner(const HTMLFrameOwnerElement*, bool clipToLayerContents) const;

    virtual IntRect windowResizerRect() const;

    virtual float visibleContentScaleFactor() const OVERRIDE { return m_visibleContentScaleFactor; }
    void setVisibleContentScaleFactor(float);

    virtual void setScrollPosition(const IntPoint&) OVERRIDE;
    virtual void repaintFixedElementsAfterScrolling();
    virtual void updateFixedElementsAfterScrolling();
    virtual bool shouldRubberBandInDirection(ScrollDirection) const;
    virtual bool requestScrollPositionUpdate(const IntPoint&) OVERRIDE;
    virtual bool isRubberBandInProgress() const OVERRIDE;
    void setScrollPositionNonProgrammatically(const IntPoint&);

    // This is different than visibleContentRect() in that it ignores negative (or overly positive)
    // offsets from rubber-banding, and it takes zooming into account.
    LayoutRect viewportConstrainedVisibleContentRect() const;

    AtomicString mediaType() const;
    void setMediaType(const AtomicString&);
    void adjustMediaTypeForPrinting(bool printing);

    void setCannotBlitToWindow();
    void setIsOverlapped(bool);
    bool isOverlapped() const { return m_isOverlapped; }
    bool isOverlappedIncludingAncestors() const;
    void setContentIsOpaque(bool);

    void addSlowRepaintObject();
    void removeSlowRepaintObject();
    bool hasSlowRepaintObjects() const { return m_slowRepaintObjectCount; }

    // Includes fixed- and sticky-position objects.
    typedef HashSet<RenderObject*> ViewportConstrainedObjectSet;
    void addViewportConstrainedObject(RenderObject*);
    void removeViewportConstrainedObject(RenderObject*);
    const ViewportConstrainedObjectSet* viewportConstrainedObjects() const { return m_viewportConstrainedObjects.get(); }
    bool hasViewportConstrainedObjects() const { return m_viewportConstrainedObjects && m_viewportConstrainedObjects->size() > 0; }

    void beginDeferredRepaints();
    void endDeferredRepaints();
    void handleLoadCompleted();
    void flushDeferredRepaints();
    void startDeferredRepaintTimer(double delay);
    void resetDeferredRepaintDelay();

    void beginDisableRepaints();
    void endDisableRepaints();
    bool repaintsDisabled() { return m_disableRepaints > 0; }

    void updateAnnotatedRegions();
    void updateControlTints();

    void restoreScrollbar();

    void scheduleEvent(PassRefPtr<Event>, PassRefPtr<Node>);
    void pauseScheduledEvents();
    void resumeScheduledEvents();
    void postLayoutTimerFired(Timer<FrameView>*);

    bool wasScrolledByUser() const;
    void setWasScrolledByUser(bool);

    bool safeToPropagateScrollToParent() const { return m_safeToPropagateScrollToParent; }
    void setSafeToPropagateScrollToParent(bool isSafe) { m_safeToPropagateScrollToParent = isSafe; }

    void addWidgetToUpdate(RenderObject*);
    void removeWidgetToUpdate(RenderObject*);

    virtual void paintContents(GraphicsContext*, const IntRect& damageRect);
    void setPaintBehavior(PaintBehavior);
    PaintBehavior paintBehavior() const;
    bool isPainting() const;
    bool hasEverPainted() const { return m_lastPaintTime; }
    void setLastPaintTime(double lastPaintTime) { m_lastPaintTime = lastPaintTime; }
    void setNodeToDraw(Node*);

    virtual void paintOverhangAreas(GraphicsContext*, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect);
    virtual void paintScrollCorner(GraphicsContext*, const IntRect& cornerRect);
    virtual void paintScrollbar(GraphicsContext*, Scrollbar*, const IntRect&) OVERRIDE;

    Color documentBackgroundColor() const;

    static double currentFrameTimeStamp() { return s_currentFrameTimeStamp; }

    void updateLayoutAndStyleIfNeededRecursive();

    void incrementVisuallyNonEmptyCharacterCount(unsigned);
    void incrementVisuallyNonEmptyPixelCount(const IntSize&);
    void setIsVisuallyNonEmpty() { m_isVisuallyNonEmpty = true; }
    bool isVisuallyNonEmpty() const { return m_isVisuallyNonEmpty; }
    void enableAutoSizeMode(bool enable, const IntSize& minSize, const IntSize& maxSize);

    void forceLayout(bool allowSubtree = false);
    void forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor, AdjustViewSizeOrNot);

    bool scrollToFragment(const KURL&);
    bool scrollToAnchor(const String&);
    void maintainScrollPositionAtAnchor(Node*);
    void scrollElementToRect(Element*, const IntRect&);

    // Methods to convert points and rects between the coordinate space of the renderer, and this view.
    virtual IntRect convertFromRenderer(const RenderObject*, const IntRect&) const;
    virtual IntRect convertToRenderer(const RenderObject*, const IntRect&) const;
    virtual IntPoint convertFromRenderer(const RenderObject*, const IntPoint&) const;
    virtual IntPoint convertToRenderer(const RenderObject*, const IntPoint&) const;

    bool isFrameViewScrollCorner(RenderScrollbarPart* scrollCorner) const { return m_scrollCorner == scrollCorner; }

    bool isScrollable();

    enum ScrollbarModesCalculationStrategy { RulesFromWebContentOnly, AnyRule };
    void calculateScrollbarModesForLayout(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy = AnyRule);

    // Normal delay
    static void setRepaintThrottlingDeferredRepaintDelay(double p);
    // Negative value would mean that first few repaints happen without a delay
    static void setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(double p);
    // The delay grows on each repaint to this maximum value
    static void setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(double p);
    // On each repaint the delay increses by this amount
    static void setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(double p);

    virtual IntPoint lastKnownMousePosition() const;

    virtual bool scrollbarsCanBeActive() const OVERRIDE;

    // FIXME: Remove this method once plugin loading is decoupled from layout.
    void flushAnyPendingPostLayoutTasks();

    virtual bool shouldSuspendScrollAnimations() const;
    virtual void scrollbarStyleChanged(int newStyle, bool forceUpdate);

    void setAnimatorsAreActive();

    RenderBox* embeddedContentBox() const;

    void setTracksRepaints(bool);
    bool isTrackingRepaints() const { return m_isTrackingRepaints; }
    void resetTrackedRepaints();
    const Vector<IntRect>& trackedRepaintRects() const { return m_trackedRepaintRects; }
    String trackedRepaintRectsAsText() const;

    typedef HashSet<ScrollableArea*> ScrollableAreaSet;
    // Returns whether the scrollable area has just been newly added.
    bool addScrollableArea(ScrollableArea*);
    // Returns whether the scrollable area has just been removed.
    bool removeScrollableArea(ScrollableArea*);
    bool containsScrollableArea(const ScrollableArea*) const;
    const ScrollableAreaSet* scrollableAreas() const { return m_scrollableAreas.get(); }

    // With CSS style "resize:" enabled, a little resizer handle will appear at the bottom
    // right of the object. We keep track of these resizer areas for checking if touches
    // (implemented using Scroll gesture) are targeting the resizer.
    typedef HashSet<RenderLayer*> ResizerAreaSet;
    void addResizerArea(RenderLayer*);
    void removeResizerArea(RenderLayer*);
    const ResizerAreaSet* resizerAreas() const { return m_resizerAreas.get(); }

    virtual void removeChild(Widget*) OVERRIDE;

    // This function exists for ports that need to handle wheel events manually.
    // On Mac WebKit1 the underlying NSScrollView just does the scrolling, but on most other platforms
    // we need this function in order to do the scroll ourselves.
    bool wheelEvent(const PlatformWheelEvent&);

    // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
    // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
    // FrameViews in the page cache, but FrameView::pagination() only affects the current
    // FrameView. FrameView::pagination() will return m_pagination if it has been set. Otherwise,
    // it will return Page::pagination() since currently there are no callers that need to
    // distinguish between the two.
    const Pagination& pagination() const;
    void setPagination(const Pagination&);

    bool inProgrammaticScroll() const { return m_inProgrammaticScroll; }
    void setInProgrammaticScroll(bool programmaticScroll) { m_inProgrammaticScroll = programmaticScroll; }

    void setHasSoftwareFilters(bool hasSoftwareFilters) { m_hasSoftwareFilters = hasSoftwareFilters; }
    bool hasSoftwareFilters() const { return m_hasSoftwareFilters; }

    virtual bool isActive() const OVERRIDE;

    // DEPRECATED: Use viewportConstrainedVisibleContentRect() instead.
    IntSize scrollOffsetForFixedPosition() const;

    PartialLayoutState& partialLayout() { return m_partialLayout; }

protected:
    virtual bool scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect);
    virtual void scrollContentsSlowPath(const IntRect& updateRect);

    virtual bool isVerticalDocument() const;
    virtual bool isFlippedDocument() const;

private:
    explicit FrameView(Frame*);

    void reset();
    void init();

    virtual bool isFrameView() const OVERRIDE { return true; }

    friend class RenderWidget;
    bool useSlowRepaints(bool considerOverlap = true) const;
    bool useSlowRepaintsIfNotOverlapped() const;
    void updateCanBlitOnScrollRecursively();
    bool contentsInCompositedLayer() const;

    void applyOverflowToViewport(RenderObject*, ScrollbarMode& hMode, ScrollbarMode& vMode);
    void applyPaginationToViewport();

    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    void paintControlTints();

    void updateCounters();
    void autoSizeIfEnabled();
    void forceLayoutParentViewIfNeeded();
    void performPreLayoutTasks();
    void performLayout(RenderObject* rootForThisLayout, bool inSubtreeLayout);
    void scheduleOrPerformPostLayoutTasks();
    void performPostLayoutTasks();

    virtual void repaintContentRectangle(const IntRect&);
    virtual void contentsResized() OVERRIDE;
    virtual void visibleContentsResized();

    // Override ScrollView methods to do point conversion via renderers, in order to
    // take transforms into account.
    virtual IntRect convertToContainingView(const IntRect&) const OVERRIDE;
    virtual IntRect convertFromContainingView(const IntRect&) const OVERRIDE;
    virtual IntPoint convertToContainingView(const IntPoint&) const OVERRIDE;
    virtual IntPoint convertFromContainingView(const IntPoint&) const OVERRIDE;

    // ScrollableArea interface
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) OVERRIDE;
    virtual void getTickmarks(Vector<IntRect>&) const OVERRIDE;
    virtual void scrollTo(const IntSize&) OVERRIDE;
    virtual ScrollableArea* enclosingScrollableArea() const OVERRIDE;
    virtual IntRect scrollableAreaBoundingBox() const OVERRIDE;
    virtual bool scrollAnimatorEnabled() const OVERRIDE;
    virtual bool usesCompositedScrolling() const OVERRIDE;
    virtual GraphicsLayer* layerForScrolling() const OVERRIDE;
    virtual GraphicsLayer* layerForHorizontalScrollbar() const OVERRIDE;
    virtual GraphicsLayer* layerForVerticalScrollbar() const OVERRIDE;
    virtual GraphicsLayer* layerForScrollCorner() const OVERRIDE;
#if ENABLE(RUBBER_BANDING)
    virtual GraphicsLayer* layerForOverhangAreas() const OVERRIDE;
#endif

    void sendResizeEventIfNeeded();

    void updateScrollableAreaSet();

    virtual void notifyPageThatContentAreaWillPaint() const;

    bool shouldUseLoadTimeDeferredRepaintDelay() const;
    void deferredRepaintTimerFired(Timer<FrameView>*);
    void doDeferredRepaints();
    void updateDeferredRepaintDelayAfterRepaint();
    double adjustedDeferredRepaintDelay() const;

    bool updateWidgets();
    void updateWidget(RenderObject*);
    void scrollToAnchor();
    void scrollPositionChanged();

    bool hasCustomScrollbars() const;

    virtual void updateScrollCorner();

    FrameView* parentFrameView() const;

    virtual AXObjectCache* axObjectCache() const;
    void removeFromAXObjectCache();

    static double s_currentFrameTimeStamp; // used for detecting decoded resource thrash in the cache
    static bool s_inPaintContents;

    LayoutSize m_size;
    LayoutSize m_margins;

    typedef HashSet<RenderObject*> RenderObjectSet;
    OwnPtr<RenderObjectSet> m_widgetUpdateSet;
    RefPtr<Frame> m_frame;

    bool m_doFullRepaint;

    bool m_canHaveScrollbars;
    bool m_cannotBlitToWindow;
    bool m_isOverlapped;
    bool m_contentIsOpaque;
    unsigned m_slowRepaintObjectCount;
    int m_borderX;
    int m_borderY;

    Timer<FrameView> m_layoutTimer;
    bool m_delayedLayout;
    RenderObject* m_layoutRoot;

    bool m_layoutSchedulingEnabled;
    bool m_inLayout;
    bool m_doingPreLayoutStyleUpdate;
    bool m_inSynchronousPostLayout;
    int m_layoutCount;
    unsigned m_nestedLayoutCount;
    Timer<FrameView> m_postLayoutTasksTimer;
    bool m_firstLayoutCallbackPending;

    bool m_firstLayout;
    bool m_isTransparent;
    Color m_baseBackgroundColor;
    IntSize m_lastViewportSize;
    float m_lastZoomFactor;

    AtomicString m_mediaType;
    AtomicString m_mediaTypeWhenNotPrinting;

    OwnPtr<FrameActionScheduler> m_actionScheduler;

    bool m_overflowStatusDirty;
    bool m_horizontalOverflow;
    bool m_verticalOverflow;
    RenderObject* m_viewportRenderer;

    Pagination m_pagination;

    bool m_wasScrolledByUser;
    bool m_inProgrammaticScroll;
    bool m_safeToPropagateScrollToParent;

    unsigned m_deferringRepaints;
    unsigned m_repaintCount;
    Vector<LayoutRect> m_repaintRects;
    Timer<FrameView> m_deferredRepaintTimer;
    double m_deferredRepaintDelay;
    double m_lastPaintTime;

    unsigned m_disableRepaints;

    bool m_isTrackingRepaints; // Used for testing.
    Vector<IntRect> m_trackedRepaintRects;

    bool m_shouldUpdateWhileOffscreen;

    unsigned m_deferSetNeedsLayouts;
    bool m_setNeedsLayoutWasDeferred;

    RefPtr<Node> m_nodeToDraw;
    PaintBehavior m_paintBehavior;
    bool m_isPainting;

    unsigned m_visuallyNonEmptyCharacterCount;
    unsigned m_visuallyNonEmptyPixelCount;
    bool m_isVisuallyNonEmpty;
    bool m_firstVisuallyNonEmptyLayoutCallbackPending;

    RefPtr<Node> m_maintainScrollPositionAnchor;

    // Renderer to hold our custom scroll corner.
    RenderScrollbarPart* m_scrollCorner;

    // If true, automatically resize the frame view around its content.
    bool m_shouldAutoSize;
    bool m_inAutoSize;
    // True if autosize has been run since m_shouldAutoSize was set.
    bool m_didRunAutosize;
    // The lower bound on the size when autosizing.
    IntSize m_minAutoSize;
    // The upper bound on the size when autosizing.
    IntSize m_maxAutoSize;

    OwnPtr<ScrollableAreaSet> m_scrollableAreas;
    OwnPtr<ResizerAreaSet> m_resizerAreas;
    OwnPtr<ViewportConstrainedObjectSet> m_viewportConstrainedObjects;

    static double s_normalDeferredRepaintDelay;
    static double s_initialDeferredRepaintDelayDuringLoading;
    static double s_maxDeferredRepaintDelayDuringLoading;
    static double s_deferredRepaintDelayIncrementDuringLoading;

    bool m_hasSoftwareFilters;

    float m_visibleContentScaleFactor;

    PartialLayoutState m_partialLayout;
};

inline void FrameView::incrementVisuallyNonEmptyCharacterCount(unsigned count)
{
    if (m_isVisuallyNonEmpty)
        return;
    m_visuallyNonEmptyCharacterCount += count;
    // Use a threshold value to prevent very small amounts of visible content from triggering didFirstVisuallyNonEmptyLayout.
    // The first few hundred characters rarely contain the interesting content of the page.
    static const unsigned visualCharacterThreshold = 200;
    if (m_visuallyNonEmptyCharacterCount > visualCharacterThreshold)
        setIsVisuallyNonEmpty();
}

inline void FrameView::incrementVisuallyNonEmptyPixelCount(const IntSize& size)
{
    if (m_isVisuallyNonEmpty)
        return;
    m_visuallyNonEmptyPixelCount += size.width() * size.height();
    // Use a threshold value to prevent very small amounts of visible content from triggering didFirstVisuallyNonEmptyLayout
    static const unsigned visualPixelThreshold = 32 * 32;
    if (m_visuallyNonEmptyPixelCount > visualPixelThreshold)
        setIsVisuallyNonEmpty();
}

inline FrameView* toFrameView(Widget* widget)
{
    ASSERT(!widget || widget->isFrameView());
    return static_cast<FrameView*>(widget);
}

inline const FrameView* toFrameView(const Widget* widget)
{
    ASSERT(!widget || widget->isFrameView());
    return static_cast<const FrameView*>(widget);
}

// This will catch anyone doing an unnecessary cast.
void toFrameView(const FrameView*);

} // namespace WebCore

#endif // FrameView_h
