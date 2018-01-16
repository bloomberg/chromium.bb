/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef PaintLayerScrollableArea_h
#define PaintLayerScrollableArea_h

#include <memory>
#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/layout/ScrollAnchor.h"
#include "core/page/scrolling/StickyPositionScrollingConstraints.h"
#include "core/paint/PaintInvalidationCapableScrollableArea.h"
#include "core/paint/PaintLayerFragment.h"
#include "core/paint/ScrollbarManager.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

enum ResizerHitTestType { kResizerForPointer, kResizerForTouch };

class ComputedStyle;
class HitTestResult;
class LayoutBox;
class LayoutScrollbarPart;
class PaintLayer;
class ScrollingCoordinator;
class StickyPositionScrollingConstraints;
class SubtreeLayoutScope;

struct CORE_EXPORT PaintLayerScrollableAreaRareData {
  USING_FAST_MALLOC(PaintLayerScrollableAreaRareData);

 public:
  PaintLayerScrollableAreaRareData();

  StickyConstraintsMap sticky_constraints_map_;
  DISALLOW_COPY_AND_ASSIGN(PaintLayerScrollableAreaRareData);
};

// PaintLayerScrollableArea represents the scrollable area of a LayoutBox.
//
// To be scrollable, an element requires ‘overflow’ != visible. Note that this
// doesn’t imply having scrollbars as you can always programmatically scroll
// when ‘overflow’ is hidden (using JavaScript's element.scrollTo or
// scrollLeft).
//
// The size and scroll origin of the scrollable area are based on layout
// dimensions. They are recomputed after layout in updateScrollDimensions.
//
// updateScrollDimensions also determines if scrollbars need to be allocated,
// destroyed or updated as a result of layout. This is based on the value of the
// 'overflow' property. Having non-overlay scrollbars automatically allocates a
// scrollcorner (m_scrollCorner), which is used to style the intersection of the
// two scrollbars.
//
// Note that scrollbars are placed based on the LayoutBox's computed
// 'direction'. See https://webkit.org/b/54623 for some context.
//
// The ‘resize' property allocates a resizer (m_resizer), which is overlaid on
// top of the scroll corner. It is used to resize an element using the mouse.
//
// The scrollbars and scroll corner can also be hardware accelerated
// and thus get their own GraphicsLayer (see the layerFor* functions).
// This only happens if the associated PaintLayer is itself composited.
//
//
// ***** OVERLAY SCROLLBARS *****
// Overlay scrollbars are painted on top of the box's content. As such they
// don't use any space in the box. Software overlay scrollbars are painted by
// PaintLayerPainter::paintOverlayScrollbars after all content as part of a
// separate tree traversal. The reason for this 2nd traversal is that they need
// to be painted on top of everything. Hardware accelerated overlay scrollbars
// are painted by their associated GraphicsLayer that sets the paint flag
// PaintLayerPaintingOverlayScrollbars.
class CORE_EXPORT PaintLayerScrollableArea final
    : public GarbageCollectedFinalized<PaintLayerScrollableArea>,
      public PaintInvalidationCapableScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(PaintLayerScrollableArea);
  friend class Internals;

 private:
  class ScrollbarManager : public blink::ScrollbarManager {
    DISALLOW_NEW();

    // Helper class to manage the life cycle of Scrollbar objects.  Some layout
    // containers (e.g., flexbox, table) run multi-pass layout on their
    // children, applying different constraints.  If a child has overflow:auto,
    // it may gain and lose scrollbars multiple times during multi-pass layout,
    // causing pointless allocation/deallocation thrashing, and potentially
    // leading to other problems (crbug.com/528940).

    // ScrollbarManager allows a ScrollableArea to delay the destruction of a
    // scrollbar that is no longer needed, until the end of multi-pass layout.
    // If the scrollbar is then re-added before multi-pass layout finishes, the
    // previously "deleted" scrollbar will be restored, rather than constructing
    // a new one.
   public:
    ScrollbarManager(PaintLayerScrollableArea& scroller)
        : blink::ScrollbarManager(scroller) {}

    void SetHasHorizontalScrollbar(bool has_scrollbar) override;
    void SetHasVerticalScrollbar(bool has_scrollbar) override;

    void DestroyDetachedScrollbars();

   protected:
    Scrollbar* CreateScrollbar(ScrollbarOrientation) override;
    void DestroyScrollbar(ScrollbarOrientation) override;

    PaintLayerScrollableArea* ScrollableArea();
  };

 public:
  // If a PreventRelayoutScope object is alive, updateAfterLayout() will not
  // re-run box layout as a result of adding or removing scrollbars.
  // Instead, it will mark the PLSA as needing relayout of its box.
  // When the last PreventRelayoutScope object is popped off the stack,
  // box().setNeedsLayout(), and box().scrollbarsChanged() for LayoutBlock's,
  // will be called as appropriate for all marked PLSA's.
  class PreventRelayoutScope {
    STACK_ALLOCATED();

   public:
    PreventRelayoutScope(SubtreeLayoutScope&);
    ~PreventRelayoutScope();

    static bool RelayoutIsPrevented() { return count_; }
    static void SetBoxNeedsLayout(PaintLayerScrollableArea&,
                                  bool had_horizontal_scrollbar,
                                  bool had_vertical_scrollbar);
    static bool RelayoutNeeded() { return count_ == 0 && relayout_needed_; }
    static void ResetRelayoutNeeded();

   private:
    static int count_;
    static SubtreeLayoutScope* layout_scope_;
    static bool relayout_needed_;
    static PersistentHeapVector<Member<PaintLayerScrollableArea>>*
        needs_relayout_;
  };

  // If a FreezeScrollbarScope object is alive, updateAfterLayout() will not
  // recompute the existence of overflow:auto scrollbars.
  class FreezeScrollbarsScope {
    STACK_ALLOCATED();

   public:
    FreezeScrollbarsScope() { count_++; }
    ~FreezeScrollbarsScope() { count_--; }

    static bool ScrollbarsAreFrozen() { return count_; }

   private:
    static int count_;
  };

  // If a DelayScrollOffsetClampScope object is alive, updateAfterLayout() will
  // not clamp scroll offsets to ensure they are in the valid range.  When the
  // last DelayScrollOffsetClampScope object is destructed, all
  // PaintLayerScrollableArea's that delayed clamping their offsets will
  // immediately clamp them.
  class DelayScrollOffsetClampScope {
    STACK_ALLOCATED();

   public:
    DelayScrollOffsetClampScope();
    ~DelayScrollOffsetClampScope();

    static bool ClampingIsDelayed() { return count_; }
    static void SetNeedsClamp(PaintLayerScrollableArea*);

   private:
    static void ClampScrollableAreas();

    static int count_;
    static PersistentHeapVector<Member<PaintLayerScrollableArea>>* needs_clamp_;
  };

  // FIXME: We should pass in the LayoutBox but this opens a window
  // for crashers during PaintLayer setup (see crbug.com/368062).
  static PaintLayerScrollableArea* Create(PaintLayer& layer) {
    return new PaintLayerScrollableArea(layer);
  }

  ~PaintLayerScrollableArea() override;
  void Dispose();

  void ForceVerticalScrollbarForFirstLayout() { SetHasVerticalScrollbar(true); }
  bool HasHorizontalScrollbar() const { return HorizontalScrollbar(); }
  bool HasVerticalScrollbar() const { return VerticalScrollbar(); }

  Scrollbar* HorizontalScrollbar() const override {
    return scrollbar_manager_.HorizontalScrollbar();
  }
  Scrollbar* VerticalScrollbar() const override {
    return scrollbar_manager_.VerticalScrollbar();
  }

  PlatformChromeClient* GetChromeClient() const override;

  SmoothScrollSequencer* GetSmoothScrollSequencer() const override;

  // For composited scrolling, we allocate an extra GraphicsLayer to hold
  // onto the scrolling content. The layer can be shifted on the GPU and
  // composited at little cost.
  // Note that this is done in CompositedLayerMapping, this function being
  // only a helper.
  GraphicsLayer* LayerForScrolling() const override;

  void DidScroll(const gfx::ScrollOffset&) override;

  // GraphicsLayers for the scrolling components.
  //
  // Any function can return nullptr if they are not accelerated.
  GraphicsLayer* LayerForHorizontalScrollbar() const override;
  GraphicsLayer* LayerForVerticalScrollbar() const override;
  GraphicsLayer* LayerForScrollCorner() const override;

  bool UsesCompositedScrolling() const override;
  bool ShouldScrollOnMainThread() const override;
  bool ShouldUseIntegerScrollOffset() const override;
  bool IsActive() const override;
  bool IsScrollCornerVisible() const override;
  IntRect ScrollCornerRect() const override;
  IntRect ConvertFromScrollbarToContainingEmbeddedContentView(
      const Scrollbar&,
      const IntRect&) const override;
  IntPoint ConvertFromScrollbarToContainingEmbeddedContentView(
      const Scrollbar&,
      const IntPoint&) const override;
  IntPoint ConvertFromContainingEmbeddedContentViewToScrollbar(
      const Scrollbar&,
      const IntPoint&) const override;
  IntPoint ConvertFromRootFrame(const IntPoint&) const override;
  int ScrollSize(ScrollbarOrientation) const override;
  IntSize ScrollOffsetInt() const override;
  ScrollOffset GetScrollOffset() const override;
  IntSize MinimumScrollOffsetInt() const override;
  IntSize MaximumScrollOffsetInt() const override;
  IntRect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override;
  IntSize ContentsSize() const override;
  void ContentsResized() override;
  bool IsScrollable() const override;
  IntPoint LastKnownMousePosition() const override;
  bool ScrollAnimatorEnabled() const override;
  bool ShouldSuspendScrollAnimations() const override;
  bool ScrollbarsCanBeActive() const override;
  void ScrollbarVisibilityChanged() override;
  void ScrollbarFrameRectChanged() override;
  IntRect ScrollableAreaBoundingBox() const override;
  void RegisterForAnimation() override;
  void DeregisterForAnimation() override;
  bool UserInputScrollable(ScrollbarOrientation) const override;
  bool ShouldPlaceVerticalScrollbarOnLeft() const override;
  int PageStep(ScrollbarOrientation) const override;
  ScrollBehavior ScrollBehaviorStyle() const override;
  CompositorAnimationHost* GetCompositorAnimationHost() const override;
  CompositorAnimationTimeline* GetCompositorAnimationTimeline() const override;
  void GetTickmarks(Vector<IntRect>&) const override;

  void VisibleSizeChanged();

  // FIXME: We shouldn't allow access to m_overflowRect outside this class.
  LayoutRect OverflowRect() const { return overflow_rect_; }

  void ScrollToAbsolutePosition(
      const FloatPoint& position,
      ScrollBehavior scroll_behavior = kScrollBehaviorInstant,
      ScrollType scroll_type = kProgrammaticScroll) {
    SetScrollOffset(position - ScrollOrigin(), scroll_type, scroll_behavior);
  }

  // This will set the scroll position without clamping, and it will do all
  // post-update work even if the scroll position didn't change.
  void SetScrollOffsetUnconditionally(const ScrollOffset&,
                                      ScrollType = kProgrammaticScroll);

  // This will set the scroll position without clamping, and it will do all
  // post-update work even if the scroll position didn't change.
  void SetScrollPositionUnconditionally(const DoublePoint&,
                                        ScrollType = kProgrammaticScroll);

  // TODO(szager): Actually run these after all of layout is finished.
  // Currently, they run at the end of box()'es layout (or after all flexbox
  // layout has finished) but while document layout is still happening.
  void UpdateAfterLayout();
  void ClampScrollOffsetAfterOverflowChange();

  void DidChangeGlobalRootScroller() override;

  void UpdateAfterStyleChange(const ComputedStyle*);
  void UpdateAfterOverflowRecalc();

  bool UpdateAfterCompositingChange() override;

  bool HasScrollbar() const {
    return HasHorizontalScrollbar() || HasVerticalScrollbar();
  }
  bool HasOverflowControls() const {
    return HasScrollbar() || ScrollCorner() || Resizer();
  }

  LayoutScrollbarPart* ScrollCorner() const override { return scroll_corner_; }

  void Resize(const IntPoint& pos, const LayoutSize& old_offset);
  IntSize OffsetFromResizeCorner(const IntPoint& absolute_point) const;

  bool InResizeMode() const { return in_resize_mode_; }
  void SetInResizeMode(bool in_resize_mode) {
    in_resize_mode_ = in_resize_mode;
  }

  LayoutUnit ScrollWidth() const;
  LayoutUnit ScrollHeight() const;
  int PixelSnappedScrollWidth() const;
  int PixelSnappedScrollHeight() const;

  int VerticalScrollbarWidth(
      OverlayScrollbarClipBehavior =
          kIgnorePlatformOverlayScrollbarSize) const override;
  int HorizontalScrollbarHeight(
      OverlayScrollbarClipBehavior =
          kIgnorePlatformOverlayScrollbarSize) const override;

  DoubleSize AdjustedScrollOffset() const {
    return ToDoubleSize(DoublePoint(ScrollOrigin()) + scroll_offset_);
  }

  void PositionOverflowControls();

  // isPointInResizeControl() is used for testing if a pointer/touch position is
  // in the resize control area.
  bool IsPointInResizeControl(const IntPoint& absolute_point,
                              ResizerHitTestType) const;
  bool HitTestOverflowControls(HitTestResult&, const IntPoint& local_point);

  bool HitTestResizerInFragments(const PaintLayerFragments&,
                                 const HitTestLocation&) const;

  // Returns the new offset, after scrolling, of the given rect in absolute
  // coordinates, clipped by the parent's client rect.
  LayoutRect ScrollIntoView(const LayoutRect&,
                            const WebScrollIntoViewParams&) override;

  // Returns true if scrollable area is in the FrameView's collection of
  // scrollable areas. This can only happen if we're scrollable, visible to hit
  // test, and do in fact overflow. This means that 'overflow: hidden' or
  // 'pointer-events: none' layers never get added to the FrameView's
  // collection.
  bool ScrollsOverflow() const { return scrolls_overflow_; }

  // Rectangle encompassing the scroll corner and resizer rect.
  IntRect ScrollCornerAndResizerRect() const final;

  void UpdateNeedsCompositedScrolling(bool layer_has_been_composited = false);
  bool NeedsCompositedScrolling() const { return needs_composited_scrolling_; }

  // These are used during compositing updates to determine if the overflow
  // controls need to be repositioned in the GraphicsLayer tree.
  void SetTopmostScrollChild(PaintLayer*);
  PaintLayer* TopmostScrollChild() const {
    DCHECK(!next_topmost_scroll_child_);
    return topmost_scroll_child_;
  }

  IntRect ResizerCornerRect(const IntRect&, ResizerHitTestType) const;

  PaintLayer* Layer() const override;

  LayoutScrollbarPart* Resizer() const override { return resizer_; }

  const IntPoint& CachedOverlayScrollbarOffset() {
    return cached_overlay_scrollbar_offset_;
  }
  void SetCachedOverlayScrollbarOffset(const IntPoint& offset) {
    cached_overlay_scrollbar_offset_ = offset;
  }

  IntRect RectForHorizontalScrollbar(const IntRect& border_box_rect) const;
  IntRect RectForVerticalScrollbar(const IntRect& border_box_rect) const;

  bool ScheduleAnimation() override;
  bool ShouldPerformScrollAnchoring() const override;
  ScrollAnchor* GetScrollAnchor() override { return &scroll_anchor_; }
  bool IsPaintLayerScrollableArea() const override { return true; }

  LayoutBox* GetLayoutBox() const override;

  FloatQuad LocalToVisibleContentQuad(const FloatQuad&,
                                      const LayoutObject*,
                                      unsigned = 0) const final;

  scoped_refptr<WebTaskRunner> GetTimerTaskRunner() const final;

  bool ShouldRebuildHorizontalScrollbarLayer() const {
    return rebuild_horizontal_scrollbar_layer_;
  }
  bool ShouldRebuildVerticalScrollbarLayer() const {
    return rebuild_vertical_scrollbar_layer_;
  }
  void ResetRebuildScrollbarLayerFlags();

  // Did DelayScrollOffsetClampScope prevent us from running
  // clampScrollOffsetsAfterLayout() in updateAfterLayout()?
  bool NeedsScrollOffsetClamp() const { return needs_scroll_offset_clamp_; }
  void SetNeedsScrollOffsetClamp(bool val) { needs_scroll_offset_clamp_ = val; }

  // Did PreventRelayoutScope prevent us from running re-layout due to
  // adding/subtracting scrollbars in updateAfterLayout()?
  bool NeedsRelayout() const { return needs_relayout_; }
  void SetNeedsRelayout(bool val) { needs_relayout_ = val; }

  // Were we laid out with a horizontal scrollbar at the time we were marked as
  // needing relayout by PreventRelayoutScope?
  bool HadHorizontalScrollbarBeforeRelayout() const {
    return had_horizontal_scrollbar_before_relayout_;
  }
  void SetHadHorizontalScrollbarBeforeRelayout(bool val) {
    had_horizontal_scrollbar_before_relayout_ = val;
  }

  // Were we laid out with a vertical scrollbar at the time we were marked as
  // needing relayout by PreventRelayoutScope?
  bool HadVerticalScrollbarBeforeRelayout() const {
    return had_vertical_scrollbar_before_relayout_;
  }
  void SetHadVerticalScrollbarBeforeRelayout(bool val) {
    had_vertical_scrollbar_before_relayout_ = val;
  }

  StickyConstraintsMap& GetStickyConstraintsMap() {
    return EnsureRareData().sticky_constraints_map_;
  }

  void InvalidateAllStickyConstraints();
  void InvalidateStickyConstraintsFor(PaintLayer*,
                                      bool needs_compositing_update = true);
  void InvalidatePaintForStickyDescendants();
  bool HasNonCompositedStickyDescendants() const;
  uint32_t GetNonCompositedMainThreadScrollingReasons() {
    return non_composited_main_thread_scrolling_reasons_;
  }

  uint64_t Id() const;

  ScrollbarTheme& GetPageScrollbarTheme() const override;

  void Trace(blink::Visitor*) override;

 private:
  explicit PaintLayerScrollableArea(PaintLayer&);

  bool HasHorizontalOverflow() const;
  bool HasVerticalOverflow() const;
  bool VisualViewportSuppliesScrollbars() const;

  bool NeedsScrollbarReconstruction() const;

  void UpdateScrollOrigin();
  void UpdateScrollDimensions();
  void UpdateScrollbarEnabledState();

  // Update the proportions used for thumb rect dimensions.
  void UpdateScrollbarProportions();

  void UpdateScrollOffset(const ScrollOffset&, ScrollType) override;
  void InvalidatePaintForScrollOffsetChange(bool offset_was_zero);

  int VerticalScrollbarStart(int min_x, int max_x) const;
  int HorizontalScrollbarStart(int min_x) const;
  IntSize ScrollbarOffset(const Scrollbar&) const;

  enum ComputeScrollbarExistenceOption { kDefault, kForbidAddingAutoBars };
  void ComputeScrollbarExistence(
      bool& needs_horizontal_scrollbar,
      bool& needs_vertical_scrollbar,
      ComputeScrollbarExistenceOption = kDefault) const;

  // If the content fits entirely in the area without auto scrollbars, returns
  // true to try to remove them. This is a heuristic and can be incorrect if the
  // content size depends on the scrollbar size (e.g., percentage sizing).
  bool TryRemovingAutoScrollbars(const bool& needs_horizontal_scrollbar,
                                 const bool& needs_vertical_scrollbar);

  // Returns true iff scrollbar existence changed.
  bool SetHasHorizontalScrollbar(bool has_scrollbar);
  bool SetHasVerticalScrollbar(bool has_scrollbar);

  void UpdateScrollCornerStyle();
  LayoutSize MinimumSizeForResizing(float zoom_factor);
  LayoutRect LayoutContentRect(IncludeScrollbarsInRect) const;

  // See comments on isPointInResizeControl.
  void UpdateResizerAreaSet();
  void UpdateResizerStyle(const ComputedStyle* old_style);

  void UpdateScrollableAreaSet();

  void UpdateCompositingLayersAfterScroll();

  ScrollingCoordinator* GetScrollingCoordinator() const;

  PaintLayerScrollableAreaRareData* RareData() { return rare_data_.get(); }
  const PaintLayerScrollableAreaRareData* RareData() const {
    return rare_data_.get();
  }

  PaintLayerScrollableAreaRareData& EnsureRareData() {
    if (!rare_data_)
      rare_data_ = std::make_unique<PaintLayerScrollableAreaRareData>();
    return *rare_data_.get();
  }

  bool ComputeNeedsCompositedScrolling(const bool, const PaintLayer*);

  IntRect CornerRect(const IntRect& bounds) const;

  PaintLayer& layer_;

  PaintLayer* next_topmost_scroll_child_;
  PaintLayer* topmost_scroll_child_;

  // Keeps track of whether the layer is currently resizing, so events can cause
  // resizing to start and stop.
  unsigned in_resize_mode_ : 1;
  unsigned scrolls_overflow_ : 1;

  // True if we are in an overflow scrollbar relayout.
  unsigned in_overflow_relayout_ : 1;

  // True if a second overflow scrollbar relayout is permitted.
  unsigned allow_second_overflow_relayout_ : 1;

  // FIXME: once cc can handle composited scrolling with clip paths, we will
  // no longer need this bit.
  unsigned needs_composited_scrolling_ : 1;

  // Set to indicate that a scrollbar layer, if present, needs to be rebuilt
  // in the next compositing update because the underlying blink::Scrollbar
  // instance has been reconstructed.
  unsigned rebuild_horizontal_scrollbar_layer_ : 1;
  unsigned rebuild_vertical_scrollbar_layer_ : 1;

  unsigned needs_scroll_offset_clamp_ : 1;
  unsigned needs_relayout_ : 1;
  unsigned had_horizontal_scrollbar_before_relayout_ : 1;
  unsigned had_vertical_scrollbar_before_relayout_ : 1;

  // The width/height of our scrolled area.
  // This is OverflowModel's layout overflow translated to physical
  // coordinates. See OverflowModel for the different overflow and
  // LayoutBoxModelObject for the coordinate systems.
  LayoutRect overflow_rect_;

  // ScrollbarManager holds the Scrollbar instances.
  ScrollbarManager scrollbar_manager_;

  // This is the offset from the beginning of content flow.
  ScrollOffset scroll_offset_;

  IntPoint cached_overlay_scrollbar_offset_;

  // LayoutObject to hold our custom scroll corner.
  LayoutScrollbarPart* scroll_corner_;

  // LayoutObject to hold our custom resizer.
  LayoutScrollbarPart* resizer_;

  ScrollAnchor scroll_anchor_;

  std::unique_ptr<PaintLayerScrollableAreaRareData> rare_data_;

  // MainThreadScrollingReason due to the properties of the LayoutObject
  uint32_t non_composited_main_thread_scrolling_reasons_;

  bool has_been_disposed_;
};

DEFINE_TYPE_CASTS(PaintLayerScrollableArea,
                  ScrollableArea,
                  scrollableArea,
                  scrollableArea->IsPaintLayerScrollableArea(),
                  scrollableArea.IsPaintLayerScrollableArea());

}  // namespace blink

#endif  // LayerScrollableArea_h
