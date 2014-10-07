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

#include "core/frame/FrameViewAutoSizeInfo.h"
#include "core/rendering/PaintPhase.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/scroll/ScrollView.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AXObjectCache;
class DocumentLifecycle;
class Cursor;
class Element;
class FloatSize;
class HTMLFrameOwnerElement;
class LocalFrame;
class KURL;
class Node;
class Page;
class RenderBox;
class RenderEmbeddedObject;
class RenderObject;
class RenderScrollbarPart;
class RenderView;
class RenderWidget;
struct CompositedSelectionBound;

typedef unsigned long long DOMTimeStamp;

// FIXME: Oilpan: move Widget (and thereby FrameView) to the heap.
class FrameView final : public ScrollView {
public:
    friend class RenderView;
    friend class Internals;

    static PassRefPtr<FrameView> create(LocalFrame*);
    static PassRefPtr<FrameView> create(LocalFrame*, const IntSize& initialSize);

    virtual ~FrameView();

    virtual HostWindow* hostWindow() const override;

    virtual void invalidateRect(const IntRect&) override;
    virtual void setFrameRect(const IntRect&) override;

    LocalFrame& frame() const
    {
        ASSERT(m_frame);
        return *m_frame;
    }

    Page* page() const;

    RenderView* renderView() const;

    virtual void setCanHaveScrollbars(bool) override;

    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation) override;

    virtual void setContentsSize(const IntSize&) override;
    IntPoint clampOffsetAtScale(const IntPoint& offset, float scale) const;

    void layout(bool allowSubtree = true);
    bool didFirstLayout() const;
    void scheduleRelayout();
    void scheduleRelayoutOfSubtree(RenderObject*);
    bool layoutPending() const;
    bool isInPerformLayout() const;

    void setCanInvalidatePaintDuringPerformLayout(bool b) { m_canInvalidatePaintDuringPerformLayout = b; }
    bool canInvalidatePaintDuringPerformLayout() const { return m_canInvalidatePaintDuringPerformLayout; }

    RenderObject* layoutRoot(bool onlyDuringLayout = false) const;
    void clearLayoutSubtreeRoot() { m_layoutSubtreeRoot = 0; }
    int layoutCount() const { return m_layoutCount; }

    bool needsLayout() const;
    void setNeedsLayout();

    void setNeedsUpdateWidgetPositions() { m_needsUpdateWidgetPositions = true; }

    // Methods for getting/setting the size Blink should use to layout the contents.
    IntSize layoutSize(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    void setLayoutSize(const IntSize&);

    // If this is set to false, the layout size will need to be explicitly set by the owner.
    // E.g. WebViewImpl sets its mainFrame's layout size manually
    void setLayoutSizeFixedToFrameSize(bool isFixed) { m_layoutSizeFixedToFrameSize = isFixed; }
    bool layoutSizeFixedToFrameSize() { return m_layoutSizeFixedToFrameSize; }

    bool needsFullPaintInvalidation() const { return m_doFullPaintInvalidation; }

    void updateAcceleratedCompositingSettings();

    void recalcOverflowAfterStyleChange();

    bool isEnclosedInCompositingLayer() const;

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

    void adjustViewSize();

    virtual IntRect windowClipRect(IncludeScrollbarsInRect = ExcludeScrollbars) const override;
    IntRect windowClipRectForFrameOwner(const HTMLFrameOwnerElement*) const;

    virtual IntRect windowResizerRect() const override;

    virtual float visibleContentScaleFactor() const override { return m_visibleContentScaleFactor; }
    void setVisibleContentScaleFactor(float);

    virtual float inputEventsScaleFactor() const override;
    virtual IntSize inputEventsOffsetForEmulation() const override;
    void setInputEventsTransformForEmulation(const IntSize&, float);

    virtual void setScrollPosition(const IntPoint&, ScrollBehavior = ScrollBehaviorInstant) override;
    virtual bool isRubberBandInProgress() const override;
    void setScrollPositionNonProgrammatically(const IntPoint&);

    // This is different than visibleContentRect() in that it ignores negative (or overly positive)
    // offsets from rubber-banding, and it takes zooming into account.
    LayoutRect viewportConstrainedVisibleContentRect() const;
    void viewportConstrainedVisibleContentSizeChanged(bool widthChanged, bool heightChanged);

    AtomicString mediaType() const;
    void setMediaType(const AtomicString&);
    void adjustMediaTypeForPrinting(bool printing);

    void addSlowRepaintObject();
    void removeSlowRepaintObject();
    bool hasSlowRepaintObjects() const { return m_slowRepaintObjectCount; }

    // Fixed-position objects.
    typedef HashSet<RenderObject*> ViewportConstrainedObjectSet;
    void addViewportConstrainedObject(RenderObject*);
    void removeViewportConstrainedObject(RenderObject*);
    const ViewportConstrainedObjectSet* viewportConstrainedObjects() const { return m_viewportConstrainedObjects.get(); }
    bool hasViewportConstrainedObjects() const { return m_viewportConstrainedObjects && m_viewportConstrainedObjects->size() > 0; }

    void handleLoadCompleted();

    void updateAnnotatedRegions();

    void restoreScrollbar();

    void postLayoutTimerFired(Timer<FrameView>*);

    bool wasScrolledByUser() const;
    void setWasScrolledByUser(bool);

    bool safeToPropagateScrollToParent() const { return m_safeToPropagateScrollToParent; }
    void setSafeToPropagateScrollToParent(bool isSafe) { m_safeToPropagateScrollToParent = isSafe; }

    void addWidget(RenderWidget*);
    void removeWidget(RenderWidget*);
    void updateWidgetPositions();

    void addWidgetToUpdate(RenderEmbeddedObject&);

    virtual void paintContents(GraphicsContext*, const IntRect& damageRect) override;
    void setPaintBehavior(PaintBehavior);
    PaintBehavior paintBehavior() const;
    bool isPainting() const;
    bool hasEverPainted() const { return m_lastPaintTime; }
    void setNodeToDraw(Node*);

    virtual void paintOverhangAreas(GraphicsContext*, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect) override;
    virtual void paintScrollCorner(GraphicsContext*, const IntRect& cornerRect) override;
    virtual void paintScrollbar(GraphicsContext*, Scrollbar*, const IntRect&) override;

    Color documentBackgroundColor() const;

    static double currentFrameTimeStamp() { return s_currentFrameTimeStamp; }

    void updateLayoutAndStyleForPainting();
    void updateLayoutAndStyleIfNeededRecursive();

    void invalidateTreeIfNeededRecursive();

    void incrementVisuallyNonEmptyCharacterCount(unsigned);
    void incrementVisuallyNonEmptyPixelCount(const IntSize&);
    void setIsVisuallyNonEmpty() { m_isVisuallyNonEmpty = true; }
    void enableAutoSizeMode(const IntSize& minSize, const IntSize& maxSize);
    void disableAutoSizeMode() { m_autoSizeInfo.clear(); }

    void forceLayout(bool allowSubtree = false);
    void forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor);

    bool scrollToFragment(const KURL&);
    bool scrollToAnchor(const String&);
    void maintainScrollPositionAtAnchor(Node*);
    void scrollElementToRect(Element*, const IntRect&);
    void scrollContentsIfNeededRecursive();

    // Methods to convert points and rects between the coordinate space of the renderer, and this view.
    IntRect convertFromRenderer(const RenderObject&, const IntRect&) const;
    IntRect convertToRenderer(const RenderObject&, const IntRect&) const;
    IntPoint convertFromRenderer(const RenderObject&, const IntPoint&) const;
    IntPoint convertToRenderer(const RenderObject&, const IntPoint&) const;

    bool isFrameViewScrollCorner(RenderScrollbarPart* scrollCorner) const { return m_scrollCorner == scrollCorner; }

    bool isScrollable();

    enum ScrollbarModesCalculationStrategy { RulesFromWebContentOnly, AnyRule };
    void calculateScrollbarModesForLayoutAndSetViewportRenderer(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy = AnyRule);

    virtual IntPoint lastKnownMousePosition() const override;
    bool shouldSetCursor() const;

    void setCursor(const Cursor&);

    virtual bool scrollbarsCanBeActive() const override;

    // FIXME: Remove this method once plugin loading is decoupled from layout.
    void flushAnyPendingPostLayoutTasks();

    virtual bool shouldSuspendScrollAnimations() const override;
    virtual void scrollbarStyleChanged() override;

    RenderBox* embeddedContentBox() const;

    void setTracksPaintInvalidations(bool);
    bool isTrackingPaintInvalidations() const { return m_isTrackingPaintInvalidations; }
    void resetTrackedPaintInvalidations();

    String trackedPaintInvalidationRectsAsText() const;

    typedef HashSet<ScrollableArea*> ScrollableAreaSet;
    void addScrollableArea(ScrollableArea*);
    void removeScrollableArea(ScrollableArea*);
    const ScrollableAreaSet* scrollableAreas() const { return m_scrollableAreas.get(); }

    // With CSS style "resize:" enabled, a little resizer handle will appear at the bottom
    // right of the object. We keep track of these resizer areas for checking if touches
    // (implemented using Scroll gesture) are targeting the resizer.
    typedef HashSet<RenderBox*> ResizerAreaSet;
    void addResizerArea(RenderBox&);
    void removeResizerArea(RenderBox&);
    const ResizerAreaSet* resizerAreas() const { return m_resizerAreas.get(); }

    virtual void setParent(Widget*) override;
    virtual void removeChild(Widget*) override;

    // This function exists for ports that need to handle wheel events manually.
    // On Mac WebKit1 the underlying NSScrollView just does the scrolling, but on most other platforms
    // we need this function in order to do the scroll ourselves.
    bool wheelEvent(const PlatformWheelEvent&);

    bool inProgrammaticScroll() const { return m_inProgrammaticScroll; }
    void setInProgrammaticScroll(bool programmaticScroll) { m_inProgrammaticScroll = programmaticScroll; }

    virtual bool isActive() const override;

    // DEPRECATED: Use viewportConstrainedVisibleContentRect() instead.
    IntSize scrollOffsetForFixedPosition() const;

    // Override scrollbar notifications to update the AXObject cache.
    virtual void didAddScrollbar(Scrollbar*, ScrollbarOrientation) override;
    virtual void willRemoveScrollbar(Scrollbar*, ScrollbarOrientation) override;

    // FIXME: This should probably be renamed as the 'inSubtreeLayout' parameter
    // passed around the FrameView layout methods can be true while this returns
    // false.
    bool isSubtreeLayout() const { return !!m_layoutSubtreeRoot; }

    // Sets the tickmarks for the FrameView, overriding the default behavior
    // which is to display the tickmarks corresponding to find results.
    // If |m_tickmarks| is empty, the default behavior is restored.
    void setTickmarks(const Vector<IntRect>& tickmarks) { m_tickmarks = tickmarks; }

    // Since the compositor can resize the viewport due to top controls and
    // commit scroll offsets before a WebView::resize occurs, we need to adjust
    // our scroll extents to prevent clamping the scroll offsets.
    void setTopControlsViewportAdjustment(float);

    virtual IntPoint maximumScrollPosition() const override;

    // ScrollableArea interface
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) override;
    virtual void getTickmarks(Vector<IntRect>&) const override;
    virtual void scrollTo(const DoublePoint&) override;
    virtual IntRect scrollableAreaBoundingBox() const override;
    virtual bool scrollAnimatorEnabled() const override;
    virtual bool usesCompositedScrolling() const override;
    virtual GraphicsLayer* layerForScrolling() const override;
    virtual GraphicsLayer* layerForHorizontalScrollbar() const override;
    virtual GraphicsLayer* layerForVerticalScrollbar() const override;
    virtual GraphicsLayer* layerForScrollCorner() const override;

protected:
    virtual bool scrollContentsFastPath(const IntSize& scrollDelta) override;
    virtual void scrollContentsSlowPath(const IntRect& updateRect) override;

    virtual bool isVerticalDocument() const override;
    virtual bool isFlippedDocument() const override;

    // Prevents creation of scrollbars. Used to prevent drawing two sets of
    // overlay scrollbars in the case of the pinch viewport.
    virtual bool scrollbarsDisabled() const override;

private:
    explicit FrameView(LocalFrame*);

    void reset();
    void init();

    virtual void frameRectsChanged() override;
    virtual bool isFrameView() const override { return true; }

    friend class RenderWidget;

    bool contentsInCompositedLayer() const;

    void applyOverflowToViewportAndSetRenderer(RenderObject*, ScrollbarMode& hMode, ScrollbarMode& vMode);
    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    void updateCounters();
    void forceLayoutParentViewIfNeeded();
    void performPreLayoutTasks();
    void performLayout(RenderObject* rootForThisLayout, bool inSubtreeLayout);
    void scheduleOrPerformPostLayoutTasks();
    void performPostLayoutTasks();

    void invalidateTreeIfNeeded();

    void gatherDebugLayoutRects(RenderObject* layoutRoot);

    DocumentLifecycle& lifecycle() const;

    virtual void contentRectangleForPaintInvalidation(const IntRect&) override;
    virtual void contentsResized() override;
    virtual void scrollbarExistenceDidChange() override;

    // Override ScrollView methods to do point conversion via renderers, in order to
    // take transforms into account.
    virtual IntRect convertToContainingView(const IntRect&) const override;
    virtual IntRect convertFromContainingView(const IntRect&) const override;
    virtual IntPoint convertToContainingView(const IntPoint&) const override;
    virtual IntPoint convertFromContainingView(const IntPoint&) const override;

    void updateWidgetPositionsIfNeeded();

    bool wasViewportResized();
    void sendResizeEventIfNeeded();

    void updateScrollableAreaSet();

    virtual void notifyPageThatContentAreaWillPaint() const override;

    void scheduleUpdateWidgetsIfNecessary();
    void updateWidgetsTimerFired(Timer<FrameView>*);
    bool updateWidgets();

    void scrollToAnchor();
    void scrollPositionChanged();
    void didScrollTimerFired(Timer<FrameView>*);

    void updateLayersAndCompositingAfterScrollIfNeeded();

    static bool computeCompositedSelectionBounds(LocalFrame&, CompositedSelectionBound& start, CompositedSelectionBound& end);
    void updateCompositedSelectionBoundsIfNeeded();

    bool hasCustomScrollbars() const;
    bool shouldUseCustomScrollbars(Element*& customScrollbarElement, LocalFrame*& customScrollbarFrame);

    virtual void updateScrollCorner() override;

    FrameView* parentFrameView() const;

    AXObjectCache* axObjectCache() const;
    void removeFromAXObjectCache();

    void setLayoutSizeInternal(const IntSize&);

    bool paintInvalidationIsAllowed() const
    {
        return !isInPerformLayout() || canInvalidatePaintDuringPerformLayout();
    }

    static double s_currentFrameTimeStamp; // used for detecting decoded resource thrash in the cache
    static bool s_inPaintContents;

    LayoutSize m_size;

    typedef WillBeHeapHashSet<RefPtrWillBeMember<RenderEmbeddedObject> > EmbeddedObjectSet;
    WillBePersistentHeapHashSet<RefPtrWillBeMember<RenderEmbeddedObject> > m_widgetUpdateSet;

    // FIXME: These are just "children" of the FrameView and should be RefPtr<Widget> instead.
    WillBePersistentHeapHashSet<RefPtrWillBeMember<RenderWidget> > m_widgets;

    // Oilpan: the use of a persistent back reference 'emulates' the
    // RefPtr-cycle that is kept between the two objects non-Oilpan.
    //
    // That cycle is broken when a LocalFrame is detached by
    // FrameLoader::detachFromParent(), it then clears its
    // FrameView's m_frame reference by calling setView(nullptr).
    RefPtrWillBePersistent<LocalFrame> m_frame;

    bool m_doFullPaintInvalidation;

    bool m_canHaveScrollbars;
    unsigned m_slowRepaintObjectCount;

    bool m_hasPendingLayout;
    RenderObject* m_layoutSubtreeRoot;

    bool m_layoutSchedulingEnabled;
    bool m_inPerformLayout;
    bool m_canInvalidatePaintDuringPerformLayout;
    bool m_inSynchronousPostLayout;
    int m_layoutCount;
    unsigned m_nestedLayoutCount;
    Timer<FrameView> m_postLayoutTasksTimer;
    Timer<FrameView> m_updateWidgetsTimer;
    bool m_firstLayoutCallbackPending;

    bool m_firstLayout;
    bool m_isTransparent;
    Color m_baseBackgroundColor;
    IntSize m_lastViewportSize;
    float m_lastZoomFactor;

    AtomicString m_mediaType;
    AtomicString m_mediaTypeWhenNotPrinting;

    bool m_overflowStatusDirty;
    bool m_horizontalOverflow;
    bool m_verticalOverflow;
    RenderObject* m_viewportRenderer;

    bool m_wasScrolledByUser;
    bool m_inProgrammaticScroll;
    bool m_safeToPropagateScrollToParent;

    double m_lastPaintTime;

    bool m_isTrackingPaintInvalidations; // Used for testing.
    Vector<IntRect> m_trackedPaintInvalidationRects;

    RefPtrWillBePersistent<Node> m_nodeToDraw;
    PaintBehavior m_paintBehavior;
    bool m_isPainting;

    unsigned m_visuallyNonEmptyCharacterCount;
    unsigned m_visuallyNonEmptyPixelCount;
    bool m_isVisuallyNonEmpty;
    bool m_firstVisuallyNonEmptyLayoutCallbackPending;

    RefPtrWillBePersistent<Node> m_maintainScrollPositionAnchor;

    // Renderer to hold our custom scroll corner.
    RawPtrWillBePersistent<RenderScrollbarPart> m_scrollCorner;

    OwnPtr<ScrollableAreaSet> m_scrollableAreas;
    OwnPtr<ResizerAreaSet> m_resizerAreas;
    OwnPtr<ViewportConstrainedObjectSet> m_viewportConstrainedObjects;
    OwnPtr<FrameViewAutoSizeInfo> m_autoSizeInfo;

    float m_visibleContentScaleFactor;
    IntSize m_inputEventsOffsetForEmulation;
    float m_inputEventsScaleFactorForEmulation;

    IntSize m_layoutSize;
    bool m_layoutSizeFixedToFrameSize;

    Timer<FrameView> m_didScrollTimer;

    Vector<IntRect> m_tickmarks;

    bool m_needsUpdateWidgetPositions;
    float m_topControlsViewportAdjustment;
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

DEFINE_TYPE_CASTS(FrameView, Widget, widget, widget->isFrameView(), widget.isFrameView());

class AllowPaintInvalidationScope {
public:
    explicit AllowPaintInvalidationScope(FrameView* view)
        : m_view(view)
        , m_originalValue(view ? view->canInvalidatePaintDuringPerformLayout() : false)
    {
        if (!m_view)
            return;

        m_view->setCanInvalidatePaintDuringPerformLayout(true);
    }

    ~AllowPaintInvalidationScope()
    {
        if (!m_view)
            return;

        m_view->setCanInvalidatePaintDuringPerformLayout(m_originalValue);
    }
private:
    FrameView* m_view;
    bool m_originalValue;
};

} // namespace blink

#endif // FrameView_h
