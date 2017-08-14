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
#include "core/layout/HitTestCache.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutState.h"
#include "platform/PODFreeListArena.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollableArea.h"
#include <memory>

namespace blink {

class LayoutQuote;
class LocalFrameView;
class PaintLayerCompositor;
class ViewFragmentationContext;

// LayoutView is the root of the layout tree and the Document's LayoutObject.
//
// It corresponds to the CSS concept of 'initial containing block' (or ICB).
// http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
//
// Its dimensions match that of the layout viewport. This viewport is used to
// size elements, in particular fixed positioned elements.
// LayoutView is always at position (0,0) relative to the document (and so isn't
// necessarily in view).
// See
// https://www.chromium.org/developers/design-documents/blink-coordinate-spaces
// about the different viewports.
//
// Because there is one LayoutView per rooted layout tree (or Frame), this class
// is used to add members shared by this tree (e.g. m_layoutState or
// m_layoutQuoteHead).
class CORE_EXPORT LayoutView final : public LayoutBlockFlow {
 public:
  explicit LayoutView(Document*);
  ~LayoutView() override;
  void WillBeDestroyed() override;

  // hitTest() will update layout, style and compositing first while
  // hitTestNoLifecycleUpdate() does not.
  bool HitTest(HitTestResult&);
  bool HitTestNoLifecycleUpdate(HitTestResult&);

  // Returns the total count of calls to HitTest, for testing.
  unsigned HitTestCount() const { return hit_test_count_; }
  unsigned HitTestCacheHits() const { return hit_test_cache_hits_; }

  void ClearHitTestCache();

  const char* GetName() const override { return "LayoutView"; }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutView || LayoutBlockFlow::IsOfType(type);
  }

  PaintLayerType LayerTypeRequired() const override {
    return kNormalPaintLayer;
  }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void UpdateLayout() override;
  void UpdateLogicalWidth() override;
  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;

  // Based on LocalFrameView::LayoutSize, but:
  // - checks for null LocalFrameView
  // - returns 0x0 if using printing layout
  // - scrollbar exclusion is compatible with root layer scrolling
  IntSize GetLayoutSize(IncludeScrollbarsInRect = kExcludeScrollbars) const;

  int ViewHeight(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    return GetLayoutSize(scrollbar_inclusion).Height();
  }
  int ViewWidth(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    return GetLayoutSize(scrollbar_inclusion).Width();
  }

  int ViewLogicalWidth(IncludeScrollbarsInRect = kExcludeScrollbars) const;
  int ViewLogicalHeight(IncludeScrollbarsInRect = kExcludeScrollbars) const;

  LayoutUnit ViewLogicalHeightForPercentages() const;

  float ZoomFactor() const;

  LocalFrameView* GetFrameView() const { return frame_view_; }
  const LayoutBox& RootBox() const;

  void UpdateAfterLayout() override;

  // See comments for the equivalent method on LayoutObject.
  bool MapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor,
                                      LayoutRect&,
                                      MapCoordinatesFlags mode,
                                      VisualRectFlags) const;

  // |ancestor| can be nullptr, which will map the rect to the main frame's
  // space, even if the main frame is remote (or has intermediate remote
  // frames in the chain).
  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags,
      VisualRectFlags) const;

  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const override;
  LayoutSize OffsetForFixedPosition(bool include_pending_scroll = false) const;

  void InvalidatePaintForViewAndCompositedLayers();

  PaintInvalidationReason InvalidatePaint(
      const PaintInvalidatorContext&) const override;

  void Paint(const PaintInfo&, const LayoutPoint&) const override;
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint&) const override;

  void ClearSelection();
  void CommitPendingSelection();

  void AbsoluteRects(Vector<IntRect>&,
                     const LayoutPoint& accumulated_offset) const override;
  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const override;

  LayoutRect ViewRect() const override;
  LayoutRect OverflowClipRect(
      const LayoutPoint& location,
      OverlayScrollbarClipBehavior =
          kIgnorePlatformOverlayScrollbarSize) const override;

  void CalculateScrollbarModes(ScrollbarMode& h_mode,
                               ScrollbarMode& v_mode) const;

  LayoutState* GetLayoutState() const { return layout_state_; }

  void UpdateHitTestResult(HitTestResult&, const LayoutPoint&) override;

  ViewFragmentationContext* FragmentationContext() const {
    return fragmentation_context_.get();
  }

  LayoutUnit PageLogicalHeight() const { return page_logical_height_; }
  void SetPageLogicalHeight(LayoutUnit height) {
    page_logical_height_ = height;
  }

  // Notification that this view moved into or out of a native window.
  void SetIsInWindow(bool);

  PaintLayerCompositor* Compositor();
  bool UsesCompositing() const;

  IntRect DocumentRect() const;

  // LayoutObject that paints the root background has background-images which
  // all have background-attachment: fixed.
  bool RootBackgroundIsEntirelyFixed() const;

  IntervalArena* GetIntervalArena();

  void SetLayoutQuoteHead(LayoutQuote* head) { layout_quote_head_ = head; }
  LayoutQuote* LayoutQuoteHead() const { return layout_quote_head_; }

  // FIXME: This is a work around because the current implementation of counters
  // requires walking the entire tree repeatedly and most pages don't actually
  // use either feature so we shouldn't take the performance hit when not
  // needed. Long term we should rewrite the counter and quotes code.
  void AddLayoutCounter() { layout_counter_count_++; }
  void RemoveLayoutCounter() {
    DCHECK_GT(layout_counter_count_, 0u);
    layout_counter_count_--;
  }
  bool HasLayoutCounters() { return layout_counter_count_; }

  bool BackgroundIsKnownToBeOpaqueInRect(
      const LayoutRect& local_rect) const override;

  // Returns the viewport size in (CSS pixels) that vh and vw units are
  // calculated from.
  FloatSize ViewportSizeForViewportUnits() const;

  void PushLayoutState(LayoutState& layout_state) {
    layout_state_ = &layout_state;
  }
  void PopLayoutState() {
    DCHECK(layout_state_);
    layout_state_ = layout_state_->Next();
  }

  LayoutRect VisualOverflowRect() const override;
  LayoutRect LocalVisualRect() const override;

  // Invalidates paint for the entire view, including composited descendants,
  // but not including child frames.
  // It is very likely you do not want to call this method.
  void SetShouldDoFullPaintInvalidationForViewAndAllDescendants();

  void SetShouldDoFullPaintInvalidationOnResizeIfNeeded(bool width_changed,
                                                        bool height_changed);

  // The document scrollbar is always on the right, even in RTL. This is to
  // prevent it from moving around on navigations.
  // TODO(skobes): This is not quite the ideal behavior, see
  // http://crbug.com/250514 and http://crbug.com/249860.
  bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const override {
    return false;
  }

  // The rootLayerScrolls setting will ultimately determine whether
  // LocalFrameView or PaintLayerScrollableArea handle the scroll.
  ScrollResult Scroll(ScrollGranularity, const FloatSize&) override;

  LayoutRect DebugRect() const override;

 private:
  void MapLocalToAncestor(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags = kApplyContainerFlip) const override;

  const LayoutObject* PushMappingToContainer(
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&) const override;
  void MapAncestorToLocal(const LayoutBoxModelObject*,
                          TransformState&,
                          MapCoordinatesFlags) const override;
  void ComputeSelfHitTestRects(Vector<LayoutRect>&,
                               const LayoutPoint& layer_offset) const override;

  bool CanHaveChildren() const override;

  void LayoutContent();
#if DCHECK_IS_ON()
  void CheckLayoutState();
#endif

  void UpdateFromStyle() override;
  bool AllowsOverflowClip() const override;

  bool ShouldUsePrintingLayout() const;

  int ViewLogicalWidthForBoxSizing() const;
  int ViewLogicalHeightForBoxSizing() const;

  bool UpdateLogicalWidthAndColumnWidth() override;

  bool PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const override;

  UntracedMember<LocalFrameView> frame_view_;

  // The page logical height.
  // This is only used during printing to split the content into pages.
  // Outside of printing, this is 0.
  LayoutUnit page_logical_height_;

  // LayoutState is an optimization used during layout.
  // |m_layoutState| will be nullptr outside of layout.
  //
  // See the class comment for more details.
  LayoutState* layout_state_;

  std::unique_ptr<ViewFragmentationContext> fragmentation_context_;
  std::unique_ptr<PaintLayerCompositor> compositor_;
  RefPtr<IntervalArena> interval_arena_;

  LayoutQuote* layout_quote_head_;
  unsigned layout_counter_count_;

  unsigned hit_test_count_;
  unsigned hit_test_cache_hits_;
  Persistent<HitTestCache> hit_test_cache_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutView, IsLayoutView());

}  // namespace blink

#endif  // LayoutView_h
