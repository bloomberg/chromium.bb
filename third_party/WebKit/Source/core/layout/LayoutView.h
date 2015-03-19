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

#ifndef LayoutView_h
#define LayoutView_h

#include "core/CoreExport.h"
#include "core/dom/Position.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutState.h"
#include "core/layout/PaintInvalidationState.h"
#include "platform/PODFreeListArena.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/OwnPtr.h"

namespace blink {

class DeprecatedPaintLayerCompositor;
class LayoutQuote;

// The root of the render tree, corresponding to the CSS initial containing block.
// It's dimensions match that of the logical viewport (which may be different from
// the visible viewport in fixed-layout mode), and it is always at position (0,0)
// relative to the document (and so isn't necessarily in view).
class CORE_EXPORT LayoutView final : public LayoutBlockFlow {
public:
    explicit LayoutView(Document*);
    virtual ~LayoutView();
    void willBeDestroyed() override;

    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    // Returns the total count of calls to HitTest, for testing.
    unsigned hitTestCount() const { return m_hitTestCount; }

    virtual const char* name() const override { return "LayoutView"; }

    virtual bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutView || LayoutBlockFlow::isOfType(type); }

    virtual DeprecatedPaintLayerType layerTypeRequired() const override { return NormalDeprecatedPaintLayer; }

    virtual bool isChildAllowed(LayoutObject*, const LayoutStyle&) const override;

    virtual void layout() override;
    virtual void updateLogicalWidth() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    virtual LayoutUnit availableLogicalHeight(AvailableLogicalHeightType) const override;

    // The same as the FrameView's layoutHeight/layoutWidth but with null check guards.
    int viewHeight(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    int viewWidth(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    int viewLogicalWidth() const
    {
        return style()->isHorizontalWritingMode() ? viewWidth(ExcludeScrollbars) : viewHeight(ExcludeScrollbars);
    }
    int viewLogicalHeight() const;
    LayoutUnit viewLogicalHeightForPercentages() const;

    float zoomFactor() const;

    FrameView* frameView() const { return m_frameView; }

    enum ViewportConstrainedPosition {
        IsNotFixedPosition,
        IsFixedPosition,
    };

    static ViewportConstrainedPosition viewportConstrainedPosition(EPosition position) { return position == FixedPosition ? IsFixedPosition : IsNotFixedPosition; }
    void mapRectToPaintInvalidationBacking(const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect&, ViewportConstrainedPosition, const PaintInvalidationState*) const;
    virtual void mapRectToPaintInvalidationBacking(const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect&, const PaintInvalidationState*) const override;
    void adjustViewportConstrainedOffset(LayoutRect&, ViewportConstrainedPosition) const;

    void invalidatePaintForRectangle(const LayoutRect&, PaintInvalidationReason) const;

    void invalidatePaintForViewAndCompositedLayers();

    virtual void paint(const PaintInfo&, const LayoutPoint&) override;
    virtual void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&) override;

    enum SelectionPaintInvalidationMode { PaintInvalidationNewXOROld, PaintInvalidationNewMinusOld };
    void setSelection(LayoutObject* start, int startPos, LayoutObject*, int endPos, SelectionPaintInvalidationMode = PaintInvalidationNewXOROld);
    void clearSelection();
    void setSelection(const FrameSelection&);
    bool hasPendingSelection() const { return m_pendingSelection->m_hasPendingSelection; }
    void commitPendingSelection();
    LayoutObject* selectionStart();
    LayoutObject* selectionEnd();
    IntRect selectionBounds();
    void selectionStartEnd(int& startPos, int& endPos);
    void invalidatePaintForSelection();

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    virtual LayoutRect viewRect() const override;

    bool shouldDoFullPaintInvalidationForNextLayout() const;
    bool doingFullPaintInvalidation() const { return m_frameView->needsFullPaintInvalidation(); }

    LayoutState* layoutState() const { return m_layoutState; }

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

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

    DeprecatedPaintLayerCompositor* compositor();
    bool usesCompositing() const;

    IntRect unscaledDocumentRect() const;
    LayoutRect backgroundRect(LayoutBox* backgroundRenderer) const;

    IntRect documentRect() const;

    // Renderer that paints the root background has background-images which all have background-attachment: fixed.
    bool rootBackgroundIsEntirelyFixed() const;

    IntervalArena* intervalArena();

    void setLayoutQuoteHead(LayoutQuote* head) { m_layoutQuoteHead = head; }
    LayoutQuote* layoutQuoteHead() const { return m_layoutQuoteHead; }

    // FIXME: This is a work around because the current implementation of counters
    // requires walking the entire tree repeatedly and most pages don't actually use either
    // feature so we shouldn't take the performance hit when not needed. Long term we should
    // rewrite the counter and quotes code.
    void addLayoutCounter() { m_layoutCounterCount++; }
    void removeLayoutCounter() { ASSERT(m_layoutCounterCount > 0); m_layoutCounterCount--; }
    bool hasLayoutCounters() { return m_layoutCounterCount; }

    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const override;

    double layoutViewportWidth() const;
    double layoutViewportHeight() const;

    void pushLayoutState(LayoutState& layoutState) { m_layoutState = &layoutState; }
    void popLayoutState() { ASSERT(m_layoutState); m_layoutState = m_layoutState->next(); }
    virtual void invalidateTreeIfNeeded(const PaintInvalidationState&) override final;

private:
    virtual void mapLocalToContainer(const LayoutBoxModelObject* paintInvalidationContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0, const PaintInvalidationState* = 0) const override;
    virtual const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const override;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const override;
    virtual void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const override;

    void layoutContent();
#if ENABLE(ASSERT)
    void checkLayoutState();
#endif

    friend class ForceHorriblySlowRectMapping;

    bool shouldUsePrintingLayout() const;

    LayoutObject* backgroundRenderer() const;

    FrameView* m_frameView;

    LayoutObject* m_selectionStart;
    LayoutObject* m_selectionEnd;

    int m_selectionStartPos;
    int m_selectionEndPos;

    LayoutUnit m_pageLogicalHeight;
    bool m_pageLogicalHeightChanged;
    LayoutState* m_layoutState;
    OwnPtr<DeprecatedPaintLayerCompositor> m_compositor;
    RefPtr<IntervalArena> m_intervalArena;

    LayoutQuote* m_layoutQuoteHead;
    unsigned m_layoutCounterCount;

    unsigned m_hitTestCount;

    class PendingSelection final : public NoBaseWillBeGarbageCollected<PendingSelection> {
    public:
        static PassOwnPtrWillBeRawPtr<PendingSelection> create()
        {
            return adoptPtrWillBeNoop(new PendingSelection);
        }

        void setSelection(const FrameSelection&);
        void clear();

        DECLARE_TRACE();

        Position m_start;
        Position m_end;
        Position m_extent;
        EAffinity m_affinity;
        bool m_hasPendingSelection : 1;
        bool m_shouldShowBlockCursor : 1;

    private:
        PendingSelection();
    };
    OwnPtrWillBePersistent<PendingSelection> m_pendingSelection;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutView, isLayoutView());

// Suspends the LayoutState cached offset and clipRect optimization. Used under transforms
// that cannot be represented by LayoutState (common in SVG) and when manipulating the render
// tree during layout in ways that can trigger paint invalidation of a non-child (e.g. when a list item
// moves its list marker around). Note that even when disabled, LayoutState is still used to
// store layoutDelta.
class ForceHorriblySlowRectMapping {
    WTF_MAKE_NONCOPYABLE(ForceHorriblySlowRectMapping);
public:
    ForceHorriblySlowRectMapping(const PaintInvalidationState* paintInvalidationState)
        : m_paintInvalidationState(paintInvalidationState)
        , m_didDisable(m_paintInvalidationState && m_paintInvalidationState->cachedOffsetsEnabled())
    {
        if (m_paintInvalidationState)
            m_paintInvalidationState->m_cachedOffsetsEnabled = false;
    }

    ~ForceHorriblySlowRectMapping()
    {
        if (m_didDisable)
            m_paintInvalidationState->m_cachedOffsetsEnabled = true;
    }
private:
    const PaintInvalidationState* m_paintInvalidationState;
    bool m_didDisable;
};

} // namespace blink

#endif // LayoutView_h
