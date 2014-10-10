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
#include "platform/Widget.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/Scrollbar.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/TemporaryChange.h"
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
class RenderPart;
class RenderScrollbarPart;
class RenderView;
struct CompositedSelectionBound;

typedef unsigned long long DOMTimeStamp;

// FIXME: Oilpan: move Widget (and thereby FrameView) to the heap.
class FrameView final : public Widget, public ScrollableArea {
public:
    friend class RenderView;
    friend class Internals;

    static PassRefPtr<FrameView> create(LocalFrame*);
    static PassRefPtr<FrameView> create(LocalFrame*, const IntSize& initialSize);

    virtual ~FrameView();

    virtual void invalidateRect(const IntRect&) override;
    virtual void setFrameRect(const IntRect&) override;

    LocalFrame& frame() const
    {
        ASSERT(m_frame);
        return *m_frame;
    }

    Page* page() const;

    RenderView* renderView() const;

    void setCanHaveScrollbars(bool);

    PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation);

    void setContentsSize(const IntSize&);
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

    IntRect windowClipRectForFrameOwner(const HTMLFrameOwnerElement*) const;

    IntRect windowResizerRect() const;

    float visibleContentScaleFactor() const { return m_visibleContentScaleFactor; }
    void setVisibleContentScaleFactor(float);

    float inputEventsScaleFactor() const;
    IntSize inputEventsOffsetForEmulation() const;
    void setInputEventsTransformForEmulation(const IntSize&, float);

    void setScrollPosition(const DoublePoint&, ScrollBehavior = ScrollBehaviorInstant);
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

    void addPart(RenderPart*);
    void removePart(RenderPart*);

    void updateWidgetPositions();

    void addPartToUpdate(RenderEmbeddedObject&);

    void paintContents(GraphicsContext*, const IntRect& damageRect);
    void setPaintBehavior(PaintBehavior);
    PaintBehavior paintBehavior() const;
    bool isPainting() const;
    bool hasEverPainted() const { return m_lastPaintTime; }
    void setNodeToDraw(Node*);

    void paintOverhangAreas(GraphicsContext*, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect);
    void paintScrollCorner(GraphicsContext*, const IntRect& cornerRect);
    void paintScrollbar(GraphicsContext*, Scrollbar*, const IntRect&);

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
    void removeChild(Widget*);

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
    void scrollTo(const DoublePoint&);
    virtual IntRect scrollableAreaBoundingBox() const override;
    virtual bool scrollAnimatorEnabled() const override;
    virtual bool usesCompositedScrolling() const override;
    virtual GraphicsLayer* layerForScrolling() const override;
    virtual GraphicsLayer* layerForHorizontalScrollbar() const override;
    virtual GraphicsLayer* layerForVerticalScrollbar() const override;
    virtual GraphicsLayer* layerForScrollCorner() const override;

    // --- ScrollView ---
    virtual int scrollSize(ScrollbarOrientation) const override;
    virtual void setScrollOffset(const IntPoint&) override;
    virtual void setScrollOffset(const DoublePoint&) override;
    virtual bool isScrollCornerVisible() const override;
    void scrollbarStyleChangedInternal();
    virtual bool userInputScrollable(ScrollbarOrientation) const override;
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const override;

    void notifyPageThatContentAreaWillPaintInternal() const;

    // The window that hosts the ScrollView. The ScrollView will communicate scrolls and repaints to the
    // host window in the window's coordinate space.
    HostWindow* hostWindow() const;

    // Returns a clip rect in host window coordinates. Used to clip the blit on a scroll.
    IntRect windowClipRect(IncludeScrollbarsInRect = ExcludeScrollbars) const;

    // Functions for child manipulation and inspection.
    const HashSet<RefPtr<Widget> >* children() const { return &m_children; }
    void addChild(PassRefPtr<Widget>);
    void removeChildInternal(Widget*);

    // If the scroll view does not use a native widget, then it will have cross-platform Scrollbars. These functions
    // can be used to obtain those scrollbars.
    virtual Scrollbar* horizontalScrollbar() const override { return m_horizontalScrollbar.get(); }
    virtual Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }
    virtual bool isScrollViewScrollbar(const Widget* child) const override { return horizontalScrollbar() == child || verticalScrollbar() == child; }

    void positionScrollbarLayers();

    // Functions for setting and retrieving the scrolling mode in each axis (horizontal/vertical). The mode has values of
    // AlwaysOff, AlwaysOn, and Auto. AlwaysOff means never show a scrollbar, AlwaysOn means always show a scrollbar.
    // Auto means show a scrollbar only when one is needed.
    // Note that for platforms with native widgets, these modes are considered advisory. In other words the underlying native
    // widget may choose not to honor the requested modes.
    void setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode, bool horizontalLock = false, bool verticalLock = false);
    void setHorizontalScrollbarMode(ScrollbarMode mode, bool lock = false) { setScrollbarModes(mode, verticalScrollbarMode(), lock, verticalScrollbarLock()); }
    void setVerticalScrollbarMode(ScrollbarMode mode, bool lock = false) { setScrollbarModes(horizontalScrollbarMode(), mode, horizontalScrollbarLock(), lock); };
    void scrollbarModes(ScrollbarMode& horizontalMode, ScrollbarMode& verticalMode) const;
    ScrollbarMode horizontalScrollbarMode() const { ScrollbarMode horizontal, vertical; scrollbarModes(horizontal, vertical); return horizontal; }
    ScrollbarMode verticalScrollbarMode() const { ScrollbarMode horizontal, vertical; scrollbarModes(horizontal, vertical); return vertical; }

    void setHorizontalScrollbarLock(bool lock = true) { m_horizontalScrollbarLock = lock; }
    bool horizontalScrollbarLock() const { return m_horizontalScrollbarLock; }
    void setVerticalScrollbarLock(bool lock = true) { m_verticalScrollbarLock = lock; }
    bool verticalScrollbarLock() const { return m_verticalScrollbarLock; }

    void setScrollingModesLock(bool lock = true) { m_horizontalScrollbarLock = m_verticalScrollbarLock = lock; }

    void setCanHaveScrollbarsInternal(bool);
    bool canHaveScrollbars() const { return horizontalScrollbarMode() != ScrollbarAlwaysOff || verticalScrollbarMode() != ScrollbarAlwaysOff; }

    // By default, paint events are clipped to the visible area.  If set to
    // false, paint events are no longer clipped.
    bool clipsPaintInvalidations() const { return m_clipsRepaints; }
    void setClipsRepaints(bool);

    // Overridden by FrameView to create custom CSS scrollbars if applicable.
    PassRefPtr<Scrollbar> createScrollbarInternal(ScrollbarOrientation);

    // The visible content rect has a location that is the scrolled offset of the document. The width and height are the viewport width
    // and height. By default the scrollbars themselves are excluded from this rectangle, but an optional boolean argument allows them to be
    // included.
    virtual IntRect visibleContentRect(IncludeScrollbarsInRect = ExcludeScrollbars) const override;
    IntSize visibleSize() const { return visibleContentRect().size(); }

    // visibleContentRect().size() is computed from unscaledVisibleContentSize() divided by the value of visibleContentScaleFactor.
    // For the main frame, visibleContentScaleFactor is equal to the page's pageScaleFactor; it's 1 otherwise.
    IntSize unscaledVisibleContentSize(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    float visibleContentScaleFactorInternal() const { return 1; }

    // Offset used to convert incoming input events while emulating device metics.
    IntSize inputEventsOffsetForEmulationInternal() const { return IntSize(); }

    // Scale used to convert incoming input events. Usually the same as visibleContentScaleFactor(), unless specifically changed.
    float inputEventsScaleFactorInternal() const { return visibleContentScaleFactor(); }

    // Functions for getting/setting the size of the document contained inside the ScrollView (as an IntSize or as individual width and height
    // values).
    virtual IntSize contentsSize() const override; // Always at least as big as the visibleWidth()/visibleHeight().
    int contentsWidth() const { return contentsSize().width(); }
    int contentsHeight() const { return contentsSize().height(); }
    void setContentsSizeInternal(const IntSize&);

    // Functions for querying the current scrolled position (both as a point, a size, or as individual X and Y values).
    // FIXME: Remove the IntPoint version. crbug.com/414283.
    virtual IntPoint scrollPosition() const override { return visibleContentRect().location(); }
    virtual DoublePoint scrollPositionDouble() const override { return m_scrollPosition; }
    // FIXME: Remove scrollOffset(). crbug.com/414283.
    IntSize scrollOffset() const { return toIntSize(visibleContentRect().location()); } // Gets the scrolled position as an IntSize. Convenient for adding to other sizes.
    DoubleSize scrollOffsetDouble() const { return DoubleSize(m_scrollPosition.x(), m_scrollPosition.y()); }
    DoubleSize pendingScrollDelta() const { return m_pendingScrollDelta; }
    virtual IntPoint minimumScrollPosition() const override; // The minimum position we can be scrolled to.
    // Adjust the passed in scroll position to keep it between the minimum and maximum positions.
    IntPoint adjustScrollPositionWithinRange(const IntPoint&) const;
    DoublePoint adjustScrollPositionWithinRange(const DoublePoint&) const;
    double scrollX() const { return scrollPositionDouble().x(); }
    double scrollY() const { return scrollPositionDouble().y(); }

    virtual IntSize overhangAmount() const override;

    void cacheCurrentScrollPosition() { m_cachedScrollPosition = scrollPositionDouble(); }
    DoublePoint cachedScrollPosition() const { return m_cachedScrollPosition; }

    // Functions for scrolling the view.
    void setScrollPositionInternal(const DoublePoint&, ScrollBehavior = ScrollBehaviorInstant);
    void scrollBy(const DoubleSize& s, ScrollBehavior behavior = ScrollBehaviorInstant)
    {
        return setScrollPosition(scrollPositionDouble() + s, behavior);
    }

    bool scroll(ScrollDirection, ScrollGranularity);

    // Scroll the actual contents of the view (either blitting or invalidating as needed).
    void scrollContents(const IntSize& scrollDelta);

    // This gives us a means of blocking painting on our scrollbars until the first layout has occurred.
    void setScrollbarsSuppressed(bool suppressed, bool repaintOnUnsuppress = false);
    bool scrollbarsSuppressed() const { return m_scrollbarsSuppressed; }

    IntPoint rootViewToContents(const IntPoint&) const;
    IntPoint contentsToRootView(const IntPoint&) const;
    IntRect rootViewToContents(const IntRect&) const;
    IntRect contentsToRootView(const IntRect&) const;

    // Event coordinates are assumed to be in the coordinate space of a window that contains
    // the entire widget hierarchy. It is up to the platform to decide what the precise definition
    // of containing window is. (For example on Mac it is the containing NSWindow.)
    IntPoint windowToContents(const IntPoint&) const;
    FloatPoint windowToContents(const FloatPoint&) const;
    IntPoint contentsToWindow(const IntPoint&) const;
    IntRect windowToContents(const IntRect&) const;
    IntRect contentsToWindow(const IntRect&) const;

    // Functions for converting to screen coordinates.
    IntRect contentsToScreen(const IntRect&) const;

    // These functions are used to enable scrollbars to avoid window resizer controls that overlap the scroll view. This happens on Mac
    // for example.
    IntRect windowResizerRectInternal() const { return IntRect(); }
    bool containsScrollbarsAvoidingResizer() const;
    void adjustScrollbarsAvoidingResizerCount(int overlapDelta);
    void windowResizerRectChanged();

    void setParentInternal(Widget*); // Updates the overlapping scrollbar count.

    // Called when our frame rect changes (or the rect/scroll position of an ancestor changes).
    void frameRectsChangedInternal();

    // Updates our scrollbars and notifies our contents of the resize.
    void setFrameRectInternal(const IntRect&);

    // For platforms that need to hit test scrollbars from within the engine's event handlers (like Win32).
    Scrollbar* scrollbarAtWindowPoint(const IntPoint& windowPoint);
    Scrollbar* scrollbarAtViewPoint(const IntPoint& viewPoint);

    virtual IntPoint convertChildToSelf(const Widget* child, const IntPoint& point) const override
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point - scrollOffset();
        newPoint.moveBy(child->location());
        return newPoint;
    }

    virtual IntPoint convertSelfToChild(const Widget* child, const IntPoint& point) const override
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point + scrollOffset();
        newPoint.moveBy(-child->location());
        return newPoint;
    }

    // Widget override. Handles painting of the contents of the view as well as the scrollbars.
    virtual void paint(GraphicsContext*, const IntRect&) override;
    void paintScrollbars(GraphicsContext*, const IntRect&);

    // Widget overrides to ensure that our children's visibility status is kept up to date when we get shown and hidden.
    virtual void show() override;
    virtual void hide() override;
    virtual void setParentVisible(bool) override;

    // Pan scrolling.
    static const int noPanScrollRadius = 15;
    void addPanScrollIcon(const IntPoint&);
    void removePanScrollIcon();
    void paintPanScrollIcon(GraphicsContext*);

    bool isPointInScrollbarCorner(const IntPoint&);
    bool scrollbarCornerPresent() const;
    virtual IntRect scrollCornerRect() const override;
    void paintScrollCornerInternal(GraphicsContext*, const IntRect& cornerRect);
    void paintScrollbarInternal(GraphicsContext*, Scrollbar*, const IntRect&);

    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const override;
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const override;
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const override;
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const override;

    void calculateAndPaintOverhangAreas(GraphicsContext*, const IntRect& dirtyRect);
    void calculateAndPaintOverhangBackground(GraphicsContext*, const IntRect& dirtyRect);

    virtual bool isScrollView() const override final { return true; }
    virtual bool isFrameView() const override { return true; }

protected:
    bool scrollContentsFastPath(const IntSize& scrollDelta);
    void scrollContentsSlowPath(const IntRect& updateRect);

    bool isVerticalDocument() const;
    bool isFlippedDocument() const;

    // Prevents creation of scrollbars. Used to prevent drawing two sets of
    // overlay scrollbars in the case of the pinch viewport.
    bool scrollbarsDisabled() const;

    // --- ScrollView ---
    // NOTE: This should only be called by the overriden setScrollOffset from ScrollableArea.
    void scrollToInternal(const DoublePoint& newPosition);

    void contentRectangleForPaintInvalidationInternal(const IntRect&);

    void paintOverhangAreasInternal(GraphicsContext*, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect);

    // These functions are used to create/destroy scrollbars.
    void setHasHorizontalScrollbar(bool);
    void setHasVerticalScrollbar(bool);

    void updateScrollCornerInternal();
    virtual void invalidateScrollCornerRect(const IntRect&) override;

    void scrollContentsIfNeeded();
    // Scroll the content by via the compositor.
    bool scrollContentsFastPathInternal(const IntSize& scrollDelta) { return true; }
    // Scroll the content by invalidating everything.
    void scrollContentsSlowPathInternal(const IntRect& updateRect);

    void setScrollOrigin(const IntPoint&, bool updatePositionAtAll, bool updatePositionSynchronously);

    // Subclassed by FrameView to check the writing-mode of the document.
    bool isVerticalDocumentInternal() const { return true; }
    bool isFlippedDocumentInternal() const { return false; }

    enum ComputeScrollbarExistenceOption {
        FirstPass,
        Incremental
    };
    void computeScrollbarExistence(bool& newHasHorizontalScrollbar, bool& newHasVerticalScrollbar, const IntSize& docSize, ComputeScrollbarExistenceOption = FirstPass) const;
    void updateScrollbarGeometry();
    IntRect adjustScrollbarRectForResizer(const IntRect&, Scrollbar*);

    // Called to update the scrollbars to accurately reflect the state of the view.
    void updateScrollbars(const DoubleSize& desiredOffset);

    IntSize excludeScrollbars(const IntSize&) const;

    class InUpdateScrollbarsScope {
    public:
        explicit InUpdateScrollbarsScope(FrameView* view)
            : m_scope(view->m_inUpdateScrollbars, true)
        { }
    private:
        TemporaryChange<bool> m_scope;
    };

    bool scrollbarsDisabledInternal() const { return false; }

private:
    explicit FrameView(LocalFrame*);

    void reset();
    void init();

    virtual void frameRectsChanged() override;

    friend class RenderPart;

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

    void contentRectangleForPaintInvalidation(const IntRect&);
    virtual void contentsResized() override;
    void scrollbarExistenceDidChange();

    // Override Widget methods to do point conversion via renderers, in order to
    // take transforms into account.
    virtual IntRect convertToContainingView(const IntRect&) const override;
    virtual IntRect convertFromContainingView(const IntRect&) const override;
    virtual IntPoint convertToContainingView(const IntPoint&) const override;
    virtual IntPoint convertFromContainingView(const IntPoint&) const override;

    void updateWidgetPositionsIfNeeded();

    bool wasViewportResized();
    void sendResizeEventIfNeeded();

    void updateScrollableAreaSet();

    void notifyPageThatContentAreaWillPaint() const;

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

    void updateScrollCorner();

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
    WillBePersistentHeapHashSet<RefPtrWillBeMember<RenderEmbeddedObject> > m_partUpdateSet;

    // FIXME: These are just "children" of the FrameView and should be RefPtr<Widget> instead.
    WillBePersistentHeapHashSet<RefPtrWillBeMember<RenderPart> > m_parts;

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

    // --- ScrollView ---
    bool adjustScrollbarExistence(ComputeScrollbarExistenceOption = FirstPass);
    void adjustScrollbarOpacity();
    // FIXME(bokan): setScrollOffset, setScrollPosition, scrollTo, scrollToOffsetWithoutAnimation,
    // notifyScrollPositionChanged...there's too many ways to scroll this class. This needs
    // some cleanup.
    void setScrollOffsetFromUpdateScrollbars(const DoubleSize&);

    RefPtr<Scrollbar> m_horizontalScrollbar;
    RefPtr<Scrollbar> m_verticalScrollbar;
    ScrollbarMode m_horizontalScrollbarMode;
    ScrollbarMode m_verticalScrollbarMode;

    bool m_horizontalScrollbarLock;
    bool m_verticalScrollbarLock;

    HashSet<RefPtr<Widget> > m_children;

    DoubleSize m_pendingScrollDelta;
    DoublePoint m_scrollPosition;
    DoublePoint m_cachedScrollPosition;
    IntSize m_contentsSize;

    int m_scrollbarsAvoidingResizer;
    bool m_scrollbarsSuppressed;

    bool m_inUpdateScrollbars;

    IntPoint m_panScrollIconPoint;
    bool m_drawPanScrollIcon;

    bool m_clipsRepaints;

    IntRect rectToCopyOnScroll() const;

    void calculateOverhangAreasForPainting(IntRect& horizontalOverhangRect, IntRect& verticalOverhangRect);
    void updateOverhangAreas();
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
