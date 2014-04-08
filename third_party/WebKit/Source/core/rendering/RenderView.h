/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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
 *
 */

#ifndef RenderView_h
#define RenderView_h

#include "core/frame/FrameView.h"
#include "core/rendering/LayoutState.h"
#include "core/rendering/RenderBlockFlow.h"
#include "platform/PODFreeListArena.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class FlowThreadController;
class RenderLayerCompositor;
class RenderQuote;

// The root of the render tree, corresponding to the CSS initial containing block.
// It's dimensions match that of the logical viewport (which may be different from
// the visible viewport in fixed-layout mode), and it is always at position (0,0)
// relative to the document (and so isn't necessarily in view).
class RenderView FINAL : public RenderBlockFlow {
public:
    explicit RenderView(Document*);
    virtual ~RenderView();

    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    virtual const char* renderName() const OVERRIDE { return "RenderView"; }

    virtual bool isRenderView() const OVERRIDE { return true; }

    virtual LayerType layerTypeRequired() const OVERRIDE { return NormalLayer; }

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;

    virtual void layout() OVERRIDE;
    virtual void updateLogicalWidth() OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;

    virtual LayoutUnit availableLogicalHeight(AvailableLogicalHeightType) const OVERRIDE;

    // The same as the FrameView's layoutHeight/layoutWidth but with null check guards.
    int viewHeight(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    int viewWidth(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    int viewLogicalWidth() const
    {
        return style()->isHorizontalWritingMode() ? viewWidth(ExcludeScrollbars) : viewHeight(ExcludeScrollbars);
    }
    int viewLogicalHeight() const;

    float zoomFactor() const;

    FrameView* frameView() const { return m_frameView; }

    virtual void computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect&, bool fixed = false) const OVERRIDE;
    void repaintViewRectangle(const LayoutRect&) const;

    void repaintViewAndCompositedLayers();

    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE;
    virtual void paintBoxDecorations(PaintInfo&, const LayoutPoint&) OVERRIDE;

    enum SelectionRepaintMode { RepaintNewXOROld, RepaintNewMinusOld, RepaintNothing };
    void setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode = RepaintNewXOROld);
    void getSelection(RenderObject*& startRenderer, int& startOffset, RenderObject*& endRenderer, int& endOffset) const;
    void clearSelection();
    RenderObject* selectionStart() const { return m_selectionStart; }
    RenderObject* selectionEnd() const { return m_selectionEnd; }
    IntRect selectionBounds(bool clipToVisibleContent = true) const;
    void selectionStartEnd(int& startPos, int& endPos) const;
    void repaintSelection() const;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const OVERRIDE;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const OVERRIDE;

    virtual LayoutRect viewRect() const OVERRIDE;

    // layoutDelta is used transiently during layout to store how far an object has moved from its
    // last layout location, in order to repaint correctly.
    // If we're doing a full repaint m_layoutState will be 0, but in that case layoutDelta doesn't matter.
    LayoutSize layoutDelta() const
    {
        ASSERT(!RuntimeEnabledFeatures::repaintAfterLayoutEnabled());
        return m_layoutState ? m_layoutState->layoutDelta() : LayoutSize();
    }
    void addLayoutDelta(const LayoutSize& delta)
    {
        ASSERT(!RuntimeEnabledFeatures::repaintAfterLayoutEnabled());
        if (m_layoutState)
            m_layoutState->addLayoutDelta(delta);
    }

#if !ASSERT_DISABLED
    bool layoutDeltaMatches(const LayoutSize& delta)
    {
        ASSERT(!RuntimeEnabledFeatures::repaintAfterLayoutEnabled());
        if (!m_layoutState)
            return false;
        return (delta.width() == m_layoutState->layoutDelta().width() || m_layoutState->layoutDeltaXSaturated()) && (delta.height() == m_layoutState->layoutDelta().height() || m_layoutState->layoutDeltaYSaturated());
    }
#endif

    bool doingFullRepaint() const { return m_frameView->needsFullRepaint(); }

    // Subtree push
    void pushLayoutState(RenderObject&);

    void popLayoutState()
    {
        LayoutState* state = m_layoutState;
        m_layoutState = state->next();
        delete state;
        popLayoutStateForCurrentFlowThread();
    }

    bool shouldDisableLayoutStateForSubtree(RenderObject&) const;

    // Returns true if layoutState should be used for its cached offset and clip.
    bool layoutStateEnabled() const { return m_layoutStateDisableCount == 0 && m_layoutState; }
    LayoutState* layoutState() const { return m_layoutState; }

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) OVERRIDE;

    LayoutUnit pageLogicalHeight() const { return m_pageLogicalHeight; }
    void setPageLogicalHeight(LayoutUnit height)
    {
        if (m_pageLogicalHeight != height) {
            m_pageLogicalHeight = height;
            m_pageLogicalHeightChanged = true;
        }
    }
    bool pageLogicalHeightChanged() const { return m_pageLogicalHeightChanged; }

    // Notification that this view moved into or out of a native window.
    void setIsInWindow(bool);

    RenderLayerCompositor* compositor();
    bool usesCompositing() const;

    IntRect unscaledDocumentRect() const;
    LayoutRect backgroundRect(RenderBox* backgroundRenderer) const;

    IntRect documentRect() const;

    // Renderer that paints the root background has background-images which all have background-attachment: fixed.
    bool rootBackgroundIsEntirelyFixed() const;

    FlowThreadController* flowThreadController();

    IntervalArena* intervalArena();

    void setRenderQuoteHead(RenderQuote* head) { m_renderQuoteHead = head; }
    RenderQuote* renderQuoteHead() const { return m_renderQuoteHead; }

    // FIXME: This is a work around because the current implementation of counters
    // requires walking the entire tree repeatedly and most pages don't actually use either
    // feature so we shouldn't take the performance hit when not needed. Long term we should
    // rewrite the counter and quotes code.
    void addRenderCounter() { m_renderCounterCount++; }
    void removeRenderCounter() { ASSERT(m_renderCounterCount > 0); m_renderCounterCount--; }
    bool hasRenderCounters() { return m_renderCounterCount; }

    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const OVERRIDE;

    double layoutViewportWidth() const;
    double layoutViewportHeight() const;

    // Suspends the LayoutState optimization. Used under transforms that cannot be represented by
    // LayoutState (common in SVG) and when manipulating the render tree during layout in ways
    // that can trigger repaint of a non-child (e.g. when a list item moves its list marker around).
    // Note that even when disabled, LayoutState is still used to store layoutDelta.
    // These functions may only be accessed by LayoutStateMaintainer or LayoutStateDisabler.
    void disableLayoutState() { m_layoutStateDisableCount++; }
    void enableLayoutState() { ASSERT(m_layoutStateDisableCount > 0); m_layoutStateDisableCount--; }

private:
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const OVERRIDE;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const OVERRIDE;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const OVERRIDE;
    virtual void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const OVERRIDE;

    bool shouldRepaint(const LayoutRect&) const;

    bool rootFillsViewportBackground(RenderBox* rootBox) const;

    // These functions may only be accessed by LayoutStateMaintainer.
    bool pushLayoutState(RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageHeight = 0, bool pageHeightChanged = false, ColumnInfo* colInfo = 0)
    {
        // We push LayoutState even if layoutState is disabled because it stores layoutDelta too.
        if ((!doingFullRepaint() || RuntimeEnabledFeatures::repaintAfterLayoutEnabled()) || m_layoutState->isPaginated() || renderer.hasColumns() || renderer.flowThreadContainingBlock()
            ) {
            pushLayoutStateForCurrentFlowThread(renderer);
            m_layoutState = new LayoutState(m_layoutState, renderer, offset, pageHeight, pageHeightChanged, colInfo);
            return true;
        }
        return false;
    }

    void layoutContent();
#ifndef NDEBUG
    void checkLayoutState();
#endif

    void positionDialog(RenderBox*);
    void positionDialogs();

    void pushLayoutStateForCurrentFlowThread(const RenderObject&);
    void popLayoutStateForCurrentFlowThread();

    friend class LayoutStateMaintainer;
    friend class LayoutStateDisabler;
    friend class RootLayoutStateScope;

    bool shouldUsePrintingLayout() const;

    FrameView* m_frameView;

    RenderObject* m_selectionStart;
    RenderObject* m_selectionEnd;

    int m_selectionStartPos;
    int m_selectionEndPos;

    LayoutUnit m_pageLogicalHeight;
    bool m_pageLogicalHeightChanged;
    LayoutState* m_layoutState;
    unsigned m_layoutStateDisableCount;
    OwnPtr<RenderLayerCompositor> m_compositor;
    OwnPtr<FlowThreadController> m_flowThreadController;
    RefPtr<IntervalArena> m_intervalArena;

    RenderQuote* m_renderQuoteHead;
    unsigned m_renderCounterCount;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderView, isRenderView());

class RootLayoutStateScope {
public:
    explicit RootLayoutStateScope(RenderView& view)
        : m_view(view)
        , m_rootLayoutState(view.pageLogicalHeight(), view.pageLogicalHeightChanged())
    {
        ASSERT(!m_view.m_layoutState);
        m_view.m_layoutState = &m_rootLayoutState;
    }

    ~RootLayoutStateScope()
    {
        ASSERT(m_view.m_layoutState == &m_rootLayoutState);
        m_view.m_layoutState = 0;
    }
private:
    RenderView& m_view;
    LayoutState m_rootLayoutState;
};

// Stack-based class to assist with LayoutState push/pop
class LayoutStateMaintainer {
    WTF_MAKE_NONCOPYABLE(LayoutStateMaintainer);
public:
    // ctor to push now
    explicit LayoutStateMaintainer(RenderBox& root, const LayoutSize& offset, LayoutUnit pageHeight = 0, bool pageHeightChanged = false, ColumnInfo* colInfo = 0)
        : m_view(*root.view())
        , m_disabled(root.shouldDisableLayoutState())
        , m_didStart(false)
        , m_didEnd(false)
        , m_didCreateLayoutState(false)
    {
        push(root, offset, pageHeight, pageHeightChanged, colInfo);
    }

    // ctor to maybe push later
    explicit LayoutStateMaintainer(RenderBox& root)
        : m_view(*root.view())
        , m_disabled(false)
        , m_didStart(false)
        , m_didEnd(false)
        , m_didCreateLayoutState(false)
    {
    }

    ~LayoutStateMaintainer()
    {
        if (m_didStart)
            pop();
        ASSERT(m_didStart == m_didEnd);   // if this fires, it means that someone did a push(), but forgot to pop().
    }

    void push(RenderBox& root, const LayoutSize& offset, LayoutUnit pageHeight = 0, bool pageHeightChanged = false, ColumnInfo* colInfo = 0)
    {
        ASSERT(!m_didStart);
        // We push state even if disabled, because we still need to store layoutDelta
        m_didCreateLayoutState = m_view.pushLayoutState(root, offset, pageHeight, pageHeightChanged, colInfo);
        if (m_disabled && m_didCreateLayoutState)
            m_view.disableLayoutState();
        m_didStart = true;
    }

    bool didPush() const { return m_didStart; }

private:
    void pop()
    {
        ASSERT(m_didStart && !m_didEnd);
        if (m_didCreateLayoutState) {
            m_view.popLayoutState();
            if (m_disabled)
                m_view.enableLayoutState();
        }

        m_didEnd = true;
    }

    RenderView& m_view;
    bool m_disabled : 1;        // true if the offset and clip part of layoutState is disabled
    bool m_didStart : 1;        // true if we did a push or disable
    bool m_didEnd : 1;          // true if we popped or re-enabled
    bool m_didCreateLayoutState : 1; // true if we actually made a layout state.
};

class LayoutStateDisabler {
    WTF_MAKE_NONCOPYABLE(LayoutStateDisabler);
public:
    LayoutStateDisabler(const RenderBox& root)
        : m_view(*root.view())
    {
        m_view.disableLayoutState();
    }

    ~LayoutStateDisabler()
    {
        m_view.enableLayoutState();
    }
private:
    RenderView& m_view;
};

} // namespace WebCore

#endif // RenderView_h
