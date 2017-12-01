/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@gmail.com>
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

#include "core/paint/PaintLayerScrollableArea.h"

#include "core/css/PseudoStyleRequest.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Node.h"
#include "core/dom/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/forms/TextControlElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBoxItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayerFragment.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/PaintLayerCompositor.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/wtf/CheckedNumeric.h"
#include "public/platform/Platform.h"
#include "public/platform/TaskType.h"

namespace blink {

namespace {

// Default value is set to 15 as the default
// minimum size used by firefox is 15x15.
static const int kDefaultMinimumWidthForResizing = 15;
static const int kDefaultMinimumHeightForResizing = 15;

}  // namespace

static LayoutRect LocalToAbsolute(LayoutBox& offset, LayoutRect rect) {
  return LayoutRect(
      offset.LocalToAbsoluteQuad(FloatQuad(FloatRect(rect)), kUseTransforms)
          .BoundingBox());
}

PaintLayerScrollableAreaRareData::PaintLayerScrollableAreaRareData() {}

const int kResizerControlExpandRatioForTouch = 2;

PaintLayerScrollableArea::PaintLayerScrollableArea(PaintLayer& layer)
    : layer_(layer),
      next_topmost_scroll_child_(nullptr),
      topmost_scroll_child_(nullptr),
      in_resize_mode_(false),
      scrolls_overflow_(false),
      in_overflow_relayout_(false),
      allow_second_overflow_relayout_(false),
      needs_composited_scrolling_(false),
      rebuild_horizontal_scrollbar_layer_(false),
      rebuild_vertical_scrollbar_layer_(false),
      needs_scroll_offset_clamp_(false),
      needs_relayout_(false),
      had_horizontal_scrollbar_before_relayout_(false),
      had_vertical_scrollbar_before_relayout_(false),
      scrollbar_manager_(*this),
      scroll_corner_(nullptr),
      resizer_(nullptr),
      scroll_anchor_(this),
      non_composited_main_thread_scrolling_reasons_(0),
      has_been_disposed_(false) {
  Node* node = Box().GetNode();
  if (node && node->IsElementNode()) {
    // We save and restore only the scrollOffset as the other scroll values are
    // recalculated.
    Element* element = ToElement(node);
    scroll_offset_ = element->SavedLayerScrollOffset();
    if (!scroll_offset_.IsZero())
      GetScrollAnimator().SetCurrentOffset(scroll_offset_);
    element->SetSavedLayerScrollOffset(ScrollOffset());
  }
  UpdateResizerAreaSet();
}

PaintLayerScrollableArea::~PaintLayerScrollableArea() {
  DCHECK(has_been_disposed_);
}

void PaintLayerScrollableArea::DidScroll(const gfx::ScrollOffset& offset) {
  ScrollableArea::DidScroll(offset);
  // This should be alive if it receives composited scroll callbacks.
  CHECK(!has_been_disposed_);
}

void PaintLayerScrollableArea::Dispose() {
  if (InResizeMode() && !Box().DocumentBeingDestroyed()) {
    if (LocalFrame* frame = Box().GetFrame())
      frame->GetEventHandler().ResizeScrollableAreaDestroyed();
  }

  if (LocalFrame* frame = Box().GetFrame()) {
    if (LocalFrameView* frame_view = frame->View()) {
      frame_view->RemoveScrollableArea(this);
      frame_view->RemoveAnimatingScrollableArea(this);
    }
  }

  non_composited_main_thread_scrolling_reasons_ = 0;

  if (ScrollingCoordinator* scrolling_coordinator = GetScrollingCoordinator())
    scrolling_coordinator->WillDestroyScrollableArea(this);

  if (!Box().DocumentBeingDestroyed()) {
    Node* node = Box().GetNode();
    // FIXME: Make setSavedLayerScrollOffset take DoubleSize. crbug.com/414283.
    if (node && node->IsElementNode())
      ToElement(node)->SetSavedLayerScrollOffset(scroll_offset_);
  }

  if (LocalFrame* frame = Box().GetFrame()) {
    if (LocalFrameView* frame_view = frame->View())
      frame_view->RemoveResizerArea(Box());
  }

  Box()
      .GetDocument()
      .GetPage()
      ->GlobalRootScrollerController()
      .DidDisposeScrollableArea(*this);

  scrollbar_manager_.Dispose();

  if (scroll_corner_)
    scroll_corner_->Destroy();
  if (resizer_)
    resizer_->Destroy();

  ClearScrollableArea();

  // Note: it is not safe to call ScrollAnchor::clear if the document is being
  // destroyed, because LayoutObjectChildList::removeChildNode skips the call to
  // willBeRemovedFromTree,
  // leaving the ScrollAnchor with a stale LayoutObject pointer.
  if (RuntimeEnabledFeatures::ScrollAnchoringEnabled() &&
      !Box().DocumentBeingDestroyed())
    scroll_anchor_.ClearSelf();

  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->DidDisposeScrollableArea(*this);

  has_been_disposed_ = true;
}

void PaintLayerScrollableArea::Trace(blink::Visitor* visitor) {
  visitor->Trace(scrollbar_manager_);
  visitor->Trace(scroll_anchor_);
  ScrollableArea::Trace(visitor);
}

PlatformChromeClient* PaintLayerScrollableArea::GetChromeClient() const {
  if (Page* page = Box().GetFrame()->GetPage())
    return &page->GetChromeClient();
  return nullptr;
}

SmoothScrollSequencer* PaintLayerScrollableArea::GetSmoothScrollSequencer()
    const {
  if (Page* page = Box().GetFrame()->GetPage())
    return page->GetSmoothScrollSequencer();
  return nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::LayerForScrolling() const {
  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->ScrollingContentsLayer()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::LayerForHorizontalScrollbar() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()
                   ->GetCompositedLayerMapping()
                   ->LayerForHorizontalScrollbar()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::LayerForVerticalScrollbar() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->LayerForVerticalScrollbar()
             : nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::LayerForScrollCorner() const {
  // See crbug.com/343132.
  DisableCompositingQueryAsserts disabler;

  return Layer()->HasCompositedLayerMapping()
             ? Layer()->GetCompositedLayerMapping()->LayerForScrollCorner()
             : nullptr;
}

bool PaintLayerScrollableArea::ShouldUseIntegerScrollOffset() const {
  Frame* frame = Box().GetFrame();
  if (frame->GetSettings() &&
      !frame->GetSettings()->GetPreferCompositingToLCDTextEnabled())
    return true;

  return ScrollableArea::ShouldUseIntegerScrollOffset();
}

bool PaintLayerScrollableArea::IsActive() const {
  Page* page = Box().GetFrame()->GetPage();
  return page && page->GetFocusController().IsActive();
}

bool PaintLayerScrollableArea::IsScrollCornerVisible() const {
  return !ScrollCornerRect().IsEmpty();
}

static int CornerStart(const LayoutBox& box,
                       int min_x,
                       int max_x,
                       int thickness) {
  if (box.ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    return min_x + box.StyleRef().BorderLeftWidth();
  return max_x - thickness - box.StyleRef().BorderRightWidth();
}

IntRect PaintLayerScrollableArea::PaintLayerScrollableArea::CornerRect(
    const IntRect& bounds) const {
  int horizontal_thickness;
  int vertical_thickness;
  if (!VerticalScrollbar() && !HorizontalScrollbar()) {
    // FIXME: This isn't right. We need to know the thickness of custom
    // scrollbars even when they don't exist in order to set the resizer square
    // size properly.
    horizontal_thickness = GetPageScrollbarTheme().ScrollbarThickness();
    vertical_thickness = horizontal_thickness;
  } else if (VerticalScrollbar() && !HorizontalScrollbar()) {
    horizontal_thickness = VerticalScrollbar()->ScrollbarThickness();
    vertical_thickness = horizontal_thickness;
  } else if (HorizontalScrollbar() && !VerticalScrollbar()) {
    vertical_thickness = HorizontalScrollbar()->ScrollbarThickness();
    horizontal_thickness = vertical_thickness;
  } else {
    horizontal_thickness = VerticalScrollbar()->ScrollbarThickness();
    vertical_thickness = HorizontalScrollbar()->ScrollbarThickness();
  }
  return IntRect(
      CornerStart(Box(), bounds.X(), bounds.MaxX(), horizontal_thickness),
      bounds.MaxY() - vertical_thickness - Box().StyleRef().BorderBottomWidth(),
      horizontal_thickness, vertical_thickness);
}

IntRect PaintLayerScrollableArea::ScrollCornerRect() const {
  // We have a scrollbar corner when a scrollbar is visible and not filling the
  // entire length of the box.
  // This happens when:
  // (a) A resizer is present and at least one scrollbar is present
  // (b) Both scrollbars are present.
  bool has_horizontal_bar = HorizontalScrollbar();
  bool has_vertical_bar = VerticalScrollbar();
  bool has_resizer = Box().Style()->Resize() != EResize::kNone;
  if ((has_horizontal_bar && has_vertical_bar) ||
      (has_resizer && (has_horizontal_bar || has_vertical_bar))) {
    return CornerRect(
        Box().PixelSnappedBorderBoxRect(Layer()->SubpixelAccumulation()));
  }
  return IntRect();
}

IntRect
PaintLayerScrollableArea::ConvertFromScrollbarToContainingEmbeddedContentView(
    const Scrollbar& scrollbar,
    const IntRect& scrollbar_rect) const {
  LayoutView* view = Box().View();
  if (!view)
    return scrollbar_rect;

  IntRect rect = scrollbar_rect;
  rect.Move(ScrollbarOffset(scrollbar));

  return view->GetFrameView()->ConvertFromLayoutItem(LayoutBoxItem(&Box()),
                                                     rect);
}

IntPoint
PaintLayerScrollableArea::ConvertFromScrollbarToContainingEmbeddedContentView(
    const Scrollbar& scrollbar,
    const IntPoint& scrollbar_point) const {
  LayoutView* view = Box().View();
  if (!view)
    return scrollbar_point;

  IntPoint point = scrollbar_point;
  point.Move(ScrollbarOffset(scrollbar));
  return view->GetFrameView()->ConvertFromLayoutItem(LayoutBoxItem(&Box()),
                                                     point);
}

IntPoint
PaintLayerScrollableArea::ConvertFromContainingEmbeddedContentViewToScrollbar(
    const Scrollbar& scrollbar,
    const IntPoint& parent_point) const {
  LayoutView* view = Box().View();
  if (!view)
    return parent_point;

  IntPoint point = view->GetFrameView()->ConvertToLayoutItem(
      LayoutBoxItem(&Box()), parent_point);

  point.Move(-ScrollbarOffset(scrollbar));
  return point;
}

IntPoint PaintLayerScrollableArea::ConvertFromRootFrame(
    const IntPoint& point_in_root_frame) const {
  LayoutView* view = Box().View();
  if (!view)
    return point_in_root_frame;

  return view->GetFrameView()->ConvertFromRootFrame(point_in_root_frame);
}

int PaintLayerScrollableArea::ScrollSize(
    ScrollbarOrientation orientation) const {
  IntSize scroll_dimensions =
      MaximumScrollOffsetInt() - MinimumScrollOffsetInt();
  return (orientation == kHorizontalScrollbar) ? scroll_dimensions.Width()
                                               : scroll_dimensions.Height();
}

void PaintLayerScrollableArea::UpdateScrollOffset(
    const ScrollOffset& new_offset,
    ScrollType scroll_type) {
  if (GetScrollOffset() == new_offset)
    return;

  bool offset_was_zero = scroll_offset_.IsZero();
  scroll_offset_ = new_offset;

  LocalFrame* frame = Box().GetFrame();
  DCHECK(frame);

  LocalFrameView* frame_view = Box().GetFrameView();
  bool is_root_layer = Layer()->IsRootLayer();

  TRACE_EVENT1("devtools.timeline", "ScrollLayer", "data",
               InspectorScrollLayerEvent::Data(&Box()));

  // FIXME(420741): Resolve circular dependency between scroll offset and
  // compositing state, and remove this disabler.
  DisableCompositingQueryAsserts disabler;

  // Update the positions of our child layers (if needed as only fixed layers
  // should be impacted by a scroll).  We don't update compositing layers,
  // because we need to do a deep update from the compositing ancestor.
  if (!frame_view->IsInPerformLayout()) {
    // If we're in the middle of layout, we'll just update layers once layout
    // has finished.
    Layer()->UpdateLayerPositionsAfterOverflowScroll();
    // Update regions, scrolling may change the clip of a particular region.
    frame_view->UpdateDocumentAnnotatedRegions();
    frame_view->SetNeedsUpdateGeometries();
    UpdateCompositingLayersAfterScroll();
  }

  const LayoutBoxModelObject& paint_invalidation_container =
      Box().ContainerForPaintInvalidation();

  FloatQuad quad_for_fake_mouse_move_event = FloatQuad(FloatRect(
      Layer()->GetLayoutObject().VisualRectIncludingCompositedScrolling(
          paint_invalidation_container)));

  quad_for_fake_mouse_move_event =
      paint_invalidation_container.LocalToAbsoluteQuad(
          quad_for_fake_mouse_move_event);
  frame->GetEventHandler().DispatchFakeMouseMoveEventSoonInQuad(
      quad_for_fake_mouse_move_event);

  if (scroll_type == kUserScroll || scroll_type == kCompositorScroll) {
    Page* page = frame->GetPage();
    if (page)
      page->GetChromeClient().ClearToolTip(*frame);
  }

  InvalidatePaintForScrollOffsetChange(offset_was_zero);

  // The scrollOffsetTranslation paint property depends on the scroll offset.
  // (see: PaintPropertyTreeBuilder.updateProperties(LocalFrameView&,...) and
  // PaintPropertyTreeBuilder.updateScrollAndScrollTranslation).
  if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled() && is_root_layer) {
    frame_view->SetNeedsPaintPropertyUpdate();
  } else {
    Box().SetNeedsPaintPropertyUpdate();
  }

  // Schedule the scroll DOM event.
  if (Box().GetNode())
    Box().GetNode()->GetDocument().EnqueueScrollEventForNode(Box().GetNode());

  if (AXObjectCache* cache = Box().GetDocument().ExistingAXObjectCache())
    cache->HandleScrollPositionChanged(&Box());
  Box().View()->ClearHitTestCache();

  // Inform the FrameLoader of the new scroll position, so it can be restored
  // when navigating back.
  if (is_root_layer) {
    frame_view->GetFrame().Loader().SaveScrollState();
    frame_view->DidChangeScrollOffset();
    if (scroll_type == kCompositorScroll || scroll_type == kUserScroll) {
      if (DocumentLoader* document_loader = frame->Loader().GetDocumentLoader())
        document_loader->GetInitialScrollState().was_scrolled_by_user = true;
    }
  }

  if (IsExplicitScrollType(scroll_type)) {
    if (scroll_type != kCompositorScroll)
      ShowOverlayScrollbars();
    frame_view->ClearFragmentAnchor();
    if (RuntimeEnabledFeatures::ScrollAnchoringEnabled())
      GetScrollAnchor()->Clear();
  }
}

void PaintLayerScrollableArea::InvalidatePaintForScrollOffsetChange(
    bool offset_was_zero) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // "background-attachment: local" causes the background of this element to
    // change position due to scroll so a paint invalidation is needed.
    // TODO(pdr): This invalidation can be removed if the local background
    // attachment is painted into the scrolling contents.
    if (ScrollsOverflow() && Box().Style()->BackgroundLayers().Attachment() ==
                                 kLocalBackgroundAttachment) {
      Box().SetShouldDoFullPaintInvalidation();
      return;
    }

    // TODO(pdr): If this is the root frame, descendants with fixed background
    // attachments need to be invalidated.

    // A scroll offset translation is still needed for overflow:hidden and there
    // is an optimization to only create this translation node when scroll
    // offset is non-zero (see: NeedsScrollOrScrollTranslation in
    // PaintPropertyTreeBuilder.cpp). Because of this optimization, gaining or
    // losing scroll offset can change whether a property exists and we have to
    // invalidate paint to ensure this property gets picked up in BlockPainter.
    bool needs_repaint_for_overflow_hidden =
        !ScrollsOverflow() && (offset_was_zero || GetScrollOffset().IsZero());
    // An invalidation is needed to ensure the interest rect is recalculated
    // so newly-scrolled-to items are repainted. We may want to set a flag on
    // PaintLayer to just check for interest rect changes instead of doing a
    // full repaint.
    bool needs_repaint_for_interest_rect = true;
    if (needs_repaint_for_overflow_hidden || needs_repaint_for_interest_rect) {
      Layer()->SetNeedsRepaint();
      return;
    }

    return;
  }

  bool requires_paint_invalidation = true;

  LocalFrameView* frame_view = Box().GetFrameView();
  bool is_root_layer = Layer()->IsRootLayer();
  if (Box().View()->Compositor()->InCompositingMode()) {
    bool only_scrolled_composited_layers =
        ScrollsOverflow() && Layer()->IsAllScrollingContentComposited() &&
        Box().Style()->BackgroundLayers().Attachment() !=
            kLocalBackgroundAttachment;

    if (UsesCompositedScrolling() || only_scrolled_composited_layers)
      requires_paint_invalidation = false;
  }

  if (!requires_paint_invalidation && is_root_layer) {
    // Some special invalidations for the root layer.
    frame_view->InvalidateBackgroundAttachmentFixedObjects();
    if (frame_view->HasViewportConstrainedObjects()) {
      if (!frame_view->InvalidateViewportConstrainedObjects())
        requires_paint_invalidation = true;
    }
    InvalidatePaintForStickyDescendants();
  }

  if (requires_paint_invalidation)
    Box().SetShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
}

IntSize PaintLayerScrollableArea::ScrollOffsetInt() const {
  return FlooredIntSize(scroll_offset_);
}

ScrollOffset PaintLayerScrollableArea::GetScrollOffset() const {
  return scroll_offset_;
}

IntSize PaintLayerScrollableArea::MinimumScrollOffsetInt() const {
  return ToIntSize(-ScrollOrigin());
}

IntSize PaintLayerScrollableArea::MaximumScrollOffsetInt() const {
  if (!Box().HasOverflowClip())
    return ToIntSize(-ScrollOrigin());

  IntSize content_size = ContentsSize();
  IntSize visible_size =
      PixelSnappedIntRect(
          Box().OverflowClipRect(Box().Location(),
                                 kIgnorePlatformAndCSSOverlayScrollbarSize))
          .Size();

  Page* page = GetLayoutBox()->GetDocument().GetPage();
  DCHECK(page);
  TopDocumentRootScrollerController& controller =
      page->GlobalRootScrollerController();

  // The global root scroller should be clipped by the top LocalFrameView rather
  // than it's overflow clipping box. This is to ensure that content exposed by
  // hiding the URL bar at the bottom of the screen is visible.
  if (this == controller.RootScrollerArea())
    visible_size = controller.RootScrollerVisibleArea();

  // TODO(skobes): We should really ASSERT that contentSize >= visibleSize
  // when we are not the root layer, but we can't because contentSize is
  // based on stale layout overflow data (http://crbug.com/576933).
  content_size = content_size.ExpandedTo(visible_size);

  return ToIntSize(-ScrollOrigin() + (content_size - visible_size));
}

void PaintLayerScrollableArea::VisibleSizeChanged() {
  ShowOverlayScrollbars();
}

LayoutRect PaintLayerScrollableArea::LayoutContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  // LayoutContentRect is conceptually the same as the box's client rect.
  LayoutSize layer_size(Layer()->Size());
  LayoutUnit border_width = Box().BorderWidth();
  LayoutUnit border_height = Box().BorderHeight();
  LayoutUnit horizontal_scrollbar_height, vertical_scrollbar_width;
  if (scrollbar_inclusion == kExcludeScrollbars) {
    horizontal_scrollbar_height = LayoutUnit(
        HorizontalScrollbar() && !HorizontalScrollbar()->IsOverlayScrollbar()
            ? HorizontalScrollbar()->ScrollbarThickness()
            : 0);
    vertical_scrollbar_width = LayoutUnit(
        VerticalScrollbar() && !VerticalScrollbar()->IsOverlayScrollbar()
            ? VerticalScrollbar()->ScrollbarThickness()
            : 0);
  }

  return LayoutRect(
      LayoutPoint(ScrollPosition()),
      LayoutSize(
          layer_size.Width() - border_width - vertical_scrollbar_width,
          layer_size.Height() - border_height - horizontal_scrollbar_height)
          .ExpandedTo(LayoutSize()));
}

IntRect PaintLayerScrollableArea::VisibleContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  LayoutRect layout_content_rect(LayoutContentRect(scrollbar_inclusion));
  // TODO(szager): It's not clear that Floor() is the right thing to do here;
  // what is the correct behavior for fractional scroll offsets?
  return IntRect(
      FlooredIntPoint(layout_content_rect.Location()),
      PixelSnappedIntSize(layout_content_rect.Size(), Box().Location()));
}

IntSize PaintLayerScrollableArea::ContentsSize() const {
  return IntSize(PixelSnappedScrollWidth(), PixelSnappedScrollHeight());
}

void PaintLayerScrollableArea::ContentsResized() {
  ScrollableArea::ContentsResized();
  // Need to update the bounds of the scroll property.
  Box().SetNeedsPaintPropertyUpdate();
}

bool PaintLayerScrollableArea::IsScrollable() const {
  return ScrollsOverflow();
}

IntPoint PaintLayerScrollableArea::LastKnownMousePosition() const {
  return Box().GetFrame()
             ? Box().GetFrame()->GetEventHandler().LastKnownMousePosition()
             : IntPoint();
}

bool PaintLayerScrollableArea::ScrollAnimatorEnabled() const {
  if (Settings* settings = Box().GetFrame()->GetSettings())
    return settings->GetScrollAnimatorEnabled();
  return false;
}

bool PaintLayerScrollableArea::ShouldSuspendScrollAnimations() const {
  LayoutView* view = Box().View();
  if (!view)
    return true;
  return view->GetFrameView()->ShouldSuspendScrollAnimations();
}

void PaintLayerScrollableArea::ScrollbarVisibilityChanged() {
  UpdateScrollbarEnabledState();

  layer_.ClearClipRects();

  if (LayoutView* view = Box().View())
    view->ClearHitTestCache();
}

void PaintLayerScrollableArea::ScrollbarFrameRectChanged() {
  // Size of non-overlay scrollbar affects overflow clip rect.
  if (!HasOverlayScrollbars())
    Box().SetNeedsPaintPropertyUpdate();
}

bool PaintLayerScrollableArea::ScrollbarsCanBeActive() const {
  LayoutView* view = Box().View();
  if (!view)
    return false;
  return view->GetFrameView()->ScrollbarsCanBeActive();
}

IntRect PaintLayerScrollableArea::ScrollableAreaBoundingBox() const {
  return Box().AbsoluteBoundingBoxRect(kTraverseDocumentBoundaries);
}

void PaintLayerScrollableArea::RegisterForAnimation() {
  if (LocalFrame* frame = Box().GetFrame()) {
    if (LocalFrameView* frame_view = frame->View())
      frame_view->AddAnimatingScrollableArea(this);
  }
}

void PaintLayerScrollableArea::DeregisterForAnimation() {
  if (LocalFrame* frame = Box().GetFrame()) {
    if (LocalFrameView* frame_view = frame->View())
      frame_view->RemoveAnimatingScrollableArea(this);
  }
}

bool PaintLayerScrollableArea::UserInputScrollable(
    ScrollbarOrientation orientation) const {
  if (Box().IsIntrinsicallyScrollable(orientation))
    return true;

  if (Box().IsLayoutView()) {
    Document& document = Box().GetDocument();
    Element* fullscreen_element = Fullscreen::FullscreenElementFrom(document);
    if (fullscreen_element && fullscreen_element != document.documentElement())
      return false;

    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(Box()).CalculateScrollbarModes(h_mode, v_mode);
    ScrollbarMode mode =
        (orientation == kHorizontalScrollbar) ? h_mode : v_mode;
    return mode == kScrollbarAuto || mode == kScrollbarAlwaysOn;
  }

  EOverflow overflow_style = (orientation == kHorizontalScrollbar)
                                 ? Box().Style()->OverflowX()
                                 : Box().Style()->OverflowY();
  return (overflow_style == EOverflow::kScroll ||
          overflow_style == EOverflow::kAuto ||
          overflow_style == EOverflow::kOverlay);
}

bool PaintLayerScrollableArea::ShouldPlaceVerticalScrollbarOnLeft() const {
  return Box().ShouldPlaceBlockDirectionScrollbarOnLogicalLeft();
}

int PaintLayerScrollableArea::PageStep(ScrollbarOrientation orientation) const {
  int length =
      (orientation == kHorizontalScrollbar) ? VisibleWidth() : VisibleHeight();
  int min_page_step = static_cast<float>(length) *
                      ScrollableArea::MinFractionToStepWhenPaging();
  int page_step = max(min_page_step, length - MaxOverlapBetweenPages());

  return max(page_step, 1);
}

LayoutBox& PaintLayerScrollableArea::Box() const {
  return *layer_.GetLayoutBox();
}

PaintLayer* PaintLayerScrollableArea::Layer() const {
  return &layer_;
}

LayoutUnit PaintLayerScrollableArea::ScrollWidth() const {
  return overflow_rect_.Width();
}

LayoutUnit PaintLayerScrollableArea::ScrollHeight() const {
  return overflow_rect_.Height();
}

int PaintLayerScrollableArea::PixelSnappedScrollWidth() const {
  return SnapSizeToPixel(ScrollWidth(),
                         Box().ClientLeft() + Box().Location().X());
}

int PaintLayerScrollableArea::PixelSnappedScrollHeight() const {
  return SnapSizeToPixel(ScrollHeight(),
                         Box().ClientTop() + Box().Location().Y());
}

void PaintLayerScrollableArea::UpdateScrollOrigin() {
  // This should do nothing prior to first layout; the if-clause will catch
  // that.
  if (OverflowRect().IsEmpty())
    return;
  LayoutPoint scrollable_overflow =
      overflow_rect_.Location() -
      LayoutSize(Box().BorderLeft(), Box().BorderTop());
  SetScrollOrigin(FlooredIntPoint(-scrollable_overflow) +
                  Box().OriginAdjustmentForScrollbars());
}

void PaintLayerScrollableArea::UpdateScrollDimensions() {
  if (overflow_rect_.Size() != Box().LayoutOverflowRect().Size())
    ContentsResized();
  overflow_rect_ = Box().LayoutOverflowRect();
  Box().FlipForWritingMode(overflow_rect_);
  UpdateScrollOrigin();
}

void PaintLayerScrollableArea::UpdateScrollbarEnabledState() {
  bool force_disable =
      GetPageScrollbarTheme().ShouldDisableInvisibleScrollbars() &&
      ScrollbarsHidden();

  if (HorizontalScrollbar())
    HorizontalScrollbar()->SetEnabled(HasHorizontalOverflow() &&
                                      !force_disable);
  if (VerticalScrollbar())
    VerticalScrollbar()->SetEnabled(HasVerticalOverflow() && !force_disable);
}

void PaintLayerScrollableArea::UpdateScrollbarProportions() {
  if (Scrollbar* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->SetProportion(VisibleWidth(), ContentsSize().Width());
  if (Scrollbar* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->SetProportion(VisibleHeight(), ContentsSize().Height());
}

void PaintLayerScrollableArea::SetScrollOffsetUnconditionally(
    const ScrollOffset& offset,
    ScrollType scroll_type) {
  CancelScrollAnimation();
  ScrollOffsetChanged(offset, scroll_type);
}

void PaintLayerScrollableArea::UpdateAfterLayout() {
  bool scrollbars_are_frozen =
      (in_overflow_relayout_ && !allow_second_overflow_relayout_) ||
      FreezeScrollbarsScope::ScrollbarsAreFrozen();
  allow_second_overflow_relayout_ = false;

  if (NeedsScrollbarReconstruction()) {
    SetHasHorizontalScrollbar(false);
    SetHasVerticalScrollbar(false);
  }

  UpdateScrollDimensions();

  bool had_horizontal_scrollbar = HasHorizontalScrollbar();
  bool had_vertical_scrollbar = HasVerticalScrollbar();

  bool needs_horizontal_scrollbar;
  bool needs_vertical_scrollbar;
  ComputeScrollbarExistence(needs_horizontal_scrollbar,
                            needs_vertical_scrollbar);

  // Removing auto scrollbars is a heuristic and can be incorrect if the content
  // size depends on the scrollbar size (e.g., sized with percentages). Removing
  // scrollbars can require two additional layout passes so this is only done on
  // the first layout (!in_overflow_layout).
  if (!in_overflow_relayout_ && !scrollbars_are_frozen &&
      TryRemovingAutoScrollbars(needs_horizontal_scrollbar,
                                needs_vertical_scrollbar)) {
    needs_horizontal_scrollbar = needs_vertical_scrollbar = false;
    allow_second_overflow_relayout_ = true;
  }

  bool horizontal_scrollbar_should_change =
      needs_horizontal_scrollbar != had_horizontal_scrollbar;
  bool vertical_scrollbar_should_change =
      needs_vertical_scrollbar != had_vertical_scrollbar;

  bool scrollbars_will_change =
      !scrollbars_are_frozen &&
      (horizontal_scrollbar_should_change || vertical_scrollbar_should_change);
  if (scrollbars_will_change) {
    SetHasHorizontalScrollbar(needs_horizontal_scrollbar);
    SetHasVerticalScrollbar(needs_vertical_scrollbar);

    if (HasScrollbar())
      UpdateScrollCornerStyle();

    Layer()->UpdateSelfPaintingLayer();

    // Force an update since we know the scrollbars have changed things.
    if (Box().GetDocument().HasAnnotatedRegions())
      Box().GetDocument().SetAnnotatedRegionsDirty(true);

    // Our proprietary overflow: overlay value doesn't trigger a layout.
    if ((horizontal_scrollbar_should_change &&
         Box().Style()->OverflowX() != EOverflow::kOverlay) ||
        (vertical_scrollbar_should_change &&
         Box().Style()->OverflowY() != EOverflow::kOverlay)) {
      if ((vertical_scrollbar_should_change &&
           Box().IsHorizontalWritingMode()) ||
          (horizontal_scrollbar_should_change &&
           !Box().IsHorizontalWritingMode())) {
        Box().SetPreferredLogicalWidthsDirty();
      }
      if (PreventRelayoutScope::RelayoutIsPrevented()) {
        // We're not doing re-layout right now, but we still want to
        // add the scrollbar to the logical width now, to facilitate parent
        // layout.
        Box().UpdateLogicalWidth();
        PreventRelayoutScope::SetBoxNeedsLayout(*this, had_horizontal_scrollbar,
                                                had_vertical_scrollbar);
      } else {
        in_overflow_relayout_ = true;
        SubtreeLayoutScope layout_scope(Box());
        layout_scope.SetNeedsLayout(
            &Box(), LayoutInvalidationReason::kScrollbarChanged);
        if (Box().IsLayoutBlock()) {
          LayoutBlock& block = ToLayoutBlock(Box());
          block.ScrollbarsChanged(horizontal_scrollbar_should_change,
                                  vertical_scrollbar_should_change);
          block.UpdateBlockLayout(true);
        } else {
          Box().UpdateLayout();
        }
        in_overflow_relayout_ = false;
        scrollbar_manager_.DestroyDetachedScrollbars();
      }
      LayoutObject* parent = Box().Parent();
      if (parent && parent->IsFlexibleBox())
        ToLayoutFlexibleBox(parent)->ClearCachedMainSizeForChild(Box());
    }
  }

  {
    // Hits in
    // compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
    DisableCompositingQueryAsserts disabler;

    UpdateScrollbarEnabledState();

    UpdateScrollbarProportions();
  }

  if (!scrollbars_are_frozen && HasOverlayScrollbars()) {
    if (!ScrollSize(kHorizontalScrollbar))
      SetHasHorizontalScrollbar(false);
    if (!ScrollSize(kVerticalScrollbar))
      SetHasVerticalScrollbar(false);
  }

  ClampScrollOffsetAfterOverflowChange();

  if (!scrollbars_are_frozen) {
    UpdateScrollableAreaSet();
  }

  DisableCompositingQueryAsserts disabler;
  PositionOverflowControls();
}

void PaintLayerScrollableArea::ClampScrollOffsetAfterOverflowChange() {
  // If a vertical scrollbar was removed, the min/max scroll offsets may have
  // changed, so the scroll offsets needs to be clamped.  If the scroll offset
  // did not change, but the scroll origin *did* change, we still need to notify
  // the scrollbars to update their dimensions.

  if (DelayScrollOffsetClampScope::ClampingIsDelayed()) {
    DelayScrollOffsetClampScope::SetNeedsClamp(this);
    return;
  }

  if (ScrollOriginChanged())
    SetScrollOffsetUnconditionally(ClampScrollOffset(GetScrollOffset()));
  else
    ScrollableArea::SetScrollOffset(GetScrollOffset(), kClampingScroll);

  SetNeedsScrollOffsetClamp(false);
  ResetScrollOriginChanged();
  scrollbar_manager_.DestroyDetachedScrollbars();
}

void PaintLayerScrollableArea::DidChangeGlobalRootScroller() {
  // Being the global root scroller will affect clipping size due to browser
  // controls behavior so we need to update compositing based on updated clip
  // geometry.
  if (Box().GetNode()->IsElementNode()) {
    ToElement(Box().GetNode())->SetNeedsCompositingUpdate();
    Box().SetNeedsPaintPropertyUpdate();
  }

  // On Android, where the VisualViewport supplies scrollbars, we need to
  // remove the PLSA's scrollbars if we become the global root scroller.
  // In general, this would be problematic as that can cause layout but this
  // should only ever apply with overlay scrollbars.
  if (Box().GetFrame()->GetSettings() &&
      Box().GetFrame()->GetSettings()->GetViewportEnabled()) {
    bool needs_horizontal_scrollbar;
    bool needs_vertical_scrollbar;
    ComputeScrollbarExistence(needs_horizontal_scrollbar,
                              needs_vertical_scrollbar);
    SetHasHorizontalScrollbar(needs_horizontal_scrollbar);
    SetHasVerticalScrollbar(needs_vertical_scrollbar);
  }
}

bool PaintLayerScrollableArea::ShouldPerformScrollAnchoring() const {
  return RuntimeEnabledFeatures::ScrollAnchoringEnabled() &&
         scroll_anchor_.HasScroller() &&
         GetLayoutBox()->Style()->OverflowAnchor() != EOverflowAnchor::kNone &&
         !Box().GetDocument().FinishingOrIsPrinting();
}

FloatQuad PaintLayerScrollableArea::LocalToVisibleContentQuad(
    const FloatQuad& quad,
    const LayoutObject* local_object,
    MapCoordinatesFlags flags) const {
  LayoutBox* box = GetLayoutBox();
  if (!box)
    return quad;
  DCHECK(local_object);
  return local_object->LocalToAncestorQuad(quad, box, flags);
}

scoped_refptr<WebTaskRunner> PaintLayerScrollableArea::GetTimerTaskRunner()
    const {
  return Box().GetFrame()->GetTaskRunner(TaskType::kUnspecedTimer);
}

ScrollBehavior PaintLayerScrollableArea::ScrollBehaviorStyle() const {
  return Box().Style()->GetScrollBehavior();
}

bool PaintLayerScrollableArea::HasHorizontalOverflow() const {
  // TODO(szager): Make the algorithm for adding/subtracting overflow:auto
  // scrollbars memoryless (crbug.com/625300).  This client_width hack will
  // prevent the spurious horizontal scrollbar, but it can cause a converse
  // problem: it can leave a sliver of horizontal overflow hidden behind the
  // vertical scrollbar without creating a horizontal scrollbar.  This
  // converse problem seems to happen much less frequently in practice, so we
  // bias the logic towards preventing unwanted horizontal scrollbars, which
  // are more common and annoying.
  LayoutUnit client_width =
      LayoutContentRect(kIncludeScrollbars).Width() -
      VerticalScrollbarWidth(kIgnorePlatformAndCSSOverlayScrollbarSize);
  if (NeedsRelayout() && !HadVerticalScrollbarBeforeRelayout())
    client_width += VerticalScrollbarWidth();
  LayoutUnit scroll_width(ScrollWidth());
  LayoutUnit box_x = Box().Location().X();
  return SnapSizeToPixel(scroll_width, box_x) >
         SnapSizeToPixel(client_width, box_x);
}

bool PaintLayerScrollableArea::HasVerticalOverflow() const {
  LayoutUnit client_height =
      LayoutContentRect(kIncludeScrollbars).Height() -
      HorizontalScrollbarHeight(kIgnorePlatformAndCSSOverlayScrollbarSize);
  LayoutUnit scroll_height(ScrollHeight());
  LayoutUnit box_y = Box().Location().Y();
  return SnapSizeToPixel(scroll_height, box_y) >
         SnapSizeToPixel(client_height, box_y);
}

// This function returns true if the given box requires overflow scrollbars (as
// opposed to the 'viewport' scrollbars managed by the PaintLayerCompositor).
// FIXME: we should use the same scrolling machinery for both the viewport and
// overflow. Currently, we need to avoid producing scrollbars here if they'll be
// handled externally in the RLC.
static bool CanHaveOverflowScrollbars(const LayoutBox& box) {
  return (RuntimeEnabledFeatures::RootLayerScrollingEnabled() ||
          !box.IsLayoutView()) &&
         box.GetDocument().ViewportDefiningElement() != box.GetNode();
}

void PaintLayerScrollableArea::UpdateAfterStyleChange(
    const ComputedStyle* old_style) {
  // Don't do this on first style recalc, before layout has ever happened.
  if (!OverflowRect().Size().IsZero()) {
    UpdateScrollableAreaSet();
  }

  // Whenever background changes on the scrollable element, the scroll bar
  // overlay style might need to be changed to have contrast against the
  // background.
  // Skip the need scrollbar check, because we dont know do we need a scrollbar
  // when this method get called.
  Color old_background;
  if (old_style) {
    old_background =
        old_style->VisitedDependentColor(CSSPropertyBackgroundColor);
  }
  Color new_background =
      Box().Style()->VisitedDependentColor(CSSPropertyBackgroundColor);

  if (new_background != old_background) {
    RecalculateScrollbarOverlayColorTheme(new_background);
  }

  bool needs_horizontal_scrollbar;
  bool needs_vertical_scrollbar;
  // We add auto scrollbars only during layout to prevent spurious activations.
  ComputeScrollbarExistence(needs_horizontal_scrollbar,
                            needs_vertical_scrollbar, kForbidAddingAutoBars);

  // Avoid some unnecessary computation if there were and will be no scrollbars.
  if (!HasScrollbar() && !needs_horizontal_scrollbar &&
      !needs_vertical_scrollbar)
    return;

  bool horizontal_scrollbar_changed =
      SetHasHorizontalScrollbar(needs_horizontal_scrollbar);
  bool vertical_scrollbar_changed =
      SetHasVerticalScrollbar(needs_vertical_scrollbar);

  if (Box().IsLayoutBlock() &&
      (horizontal_scrollbar_changed || vertical_scrollbar_changed)) {
    ToLayoutBlock(Box()).ScrollbarsChanged(
        horizontal_scrollbar_changed, vertical_scrollbar_changed,
        LayoutBlock::ScrollbarChangeContext::kStyleChange);
  }

  // With overflow: scroll, scrollbars are always visible but may be disabled.
  // When switching to another value, we need to re-enable them (see bug 11985).
  if (HasHorizontalScrollbar() && old_style &&
      old_style->OverflowX() == EOverflow::kScroll &&
      Box().Style()->OverflowX() != EOverflow::kScroll) {
    HorizontalScrollbar()->SetEnabled(true);
  }

  if (HasVerticalScrollbar() && old_style &&
      old_style->OverflowY() == EOverflow::kScroll &&
      Box().Style()->OverflowY() != EOverflow::kScroll) {
    VerticalScrollbar()->SetEnabled(true);
  }

  // FIXME: Need to detect a swap from custom to native scrollbars (and vice
  // versa).
  if (HorizontalScrollbar())
    HorizontalScrollbar()->StyleChanged();
  if (VerticalScrollbar())
    VerticalScrollbar()->StyleChanged();

  UpdateScrollCornerStyle();
  UpdateResizerAreaSet();
  UpdateResizerStyle(old_style);
}

bool PaintLayerScrollableArea::UpdateAfterCompositingChange() {
  Layer()->UpdateScrollingStateAfterCompositingChange();
  const bool layers_changed =
      topmost_scroll_child_ != next_topmost_scroll_child_;
  topmost_scroll_child_ = next_topmost_scroll_child_;
  next_topmost_scroll_child_ = nullptr;
  return layers_changed;
}

void PaintLayerScrollableArea::UpdateAfterOverflowRecalc() {
  UpdateScrollDimensions();
  UpdateScrollbarProportions();

  bool needs_horizontal_scrollbar;
  bool needs_vertical_scrollbar;
  ComputeScrollbarExistence(needs_horizontal_scrollbar,
                            needs_vertical_scrollbar);

  bool horizontal_scrollbar_should_change =
      needs_horizontal_scrollbar != HasHorizontalScrollbar();
  bool vertical_scrollbar_should_change =
      needs_vertical_scrollbar != HasVerticalScrollbar();

  if ((Box().HasAutoHorizontalScrollbar() &&
       horizontal_scrollbar_should_change) ||
      (Box().HasAutoVerticalScrollbar() && vertical_scrollbar_should_change)) {
    Box().SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kUnknown);
  }

  ClampScrollOffsetAfterOverflowChange();
}

IntRect PaintLayerScrollableArea::RectForHorizontalScrollbar(
    const IntRect& border_box_rect) const {
  if (!HasHorizontalScrollbar())
    return IntRect();

  const IntRect& scroll_corner = ScrollCornerRect();

  return IntRect(HorizontalScrollbarStart(border_box_rect.X()),
                 border_box_rect.MaxY() - Box().BorderBottom().ToInt() -
                     HorizontalScrollbar()->ScrollbarThickness(),
                 border_box_rect.Width() -
                     (Box().BorderLeft() + Box().BorderRight()).ToInt() -
                     scroll_corner.Width(),
                 HorizontalScrollbar()->ScrollbarThickness());
}

IntRect PaintLayerScrollableArea::RectForVerticalScrollbar(
    const IntRect& border_box_rect) const {
  if (!HasVerticalScrollbar())
    return IntRect();

  const IntRect& scroll_corner = ScrollCornerRect();

  return IntRect(
      VerticalScrollbarStart(border_box_rect.X(), border_box_rect.MaxX()),
      border_box_rect.Y() + Box().BorderTop().ToInt(),
      VerticalScrollbar()->ScrollbarThickness(),
      border_box_rect.Height() -
          (Box().BorderTop() + Box().BorderBottom()).ToInt() -
          scroll_corner.Height());
}

int PaintLayerScrollableArea::VerticalScrollbarStart(int min_x,
                                                     int max_x) const {
  if (Box().ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    return min_x + Box().BorderLeft().ToInt();
  return max_x - Box().BorderRight().ToInt() -
         VerticalScrollbar()->ScrollbarThickness();
}

int PaintLayerScrollableArea::HorizontalScrollbarStart(int min_x) const {
  int x = min_x + Box().BorderLeft().ToInt();
  if (Box().ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    x += HasVerticalScrollbar()
             ? VerticalScrollbar()->ScrollbarThickness()
             : ResizerCornerRect(Box().PixelSnappedBorderBoxRect(
                                     Layer()->SubpixelAccumulation()),
                                 kResizerForPointer)
                   .Width();
  return x;
}

IntSize PaintLayerScrollableArea::ScrollbarOffset(
    const Scrollbar& scrollbar) const {
  if (&scrollbar == VerticalScrollbar()) {
    return IntSize(
        VerticalScrollbarStart(0, Layer()->PixelSnappedSize().Width()),
        Box().BorderTop().ToInt());
  }

  if (&scrollbar == HorizontalScrollbar()) {
    return IntSize(HorizontalScrollbarStart(0),
                   Box().BorderTop().ToInt() + VisibleHeight());
  }

  NOTREACHED();
  return IntSize();
}

static inline const LayoutObject& ScrollbarStyleSource(
    const LayoutObject& layout_object) {
  if (Node* node = layout_object.GetNode()) {
    if (layout_object.IsLayoutView()) {
      Document& doc = node->GetDocument();
      if (Settings* settings = doc.GetSettings()) {
        if (!settings->GetAllowCustomScrollbarInMainFrame() &&
            layout_object.GetFrame() && layout_object.GetFrame()->IsMainFrame())
          return layout_object;
      }

      // Try the <body> element first as a scrollbar source.
      Element* body = doc.body();
      if (body && body->GetLayoutObject() &&
          body->GetLayoutObject()->Style()->HasPseudoStyle(kPseudoIdScrollbar))
        return *body->GetLayoutObject();

      // If the <body> didn't have a custom style, then the root element might.
      Element* doc_element = doc.documentElement();
      if (doc_element && doc_element->GetLayoutObject() &&
          doc_element->GetLayoutObject()->Style()->HasPseudoStyle(
              kPseudoIdScrollbar))
        return *doc_element->GetLayoutObject();
    }

    if (layout_object.StyleRef().HasPseudoStyle(kPseudoIdScrollbar))
      return layout_object;

    if (ShadowRoot* shadow_root = node->ContainingShadowRoot()) {
      if (shadow_root->GetType() == ShadowRootType::kUserAgent) {
        if (LayoutObject* host_layout_object =
                shadow_root->host().GetLayoutObject())
          return *host_layout_object;
      }
    }
  }

  return layout_object;
}

bool PaintLayerScrollableArea::NeedsScrollbarReconstruction() const {
  if (!HasScrollbar())
    return false;

  const LayoutObject& style_source = ScrollbarStyleSource(Box());
  bool needs_custom =
      style_source.IsBox() &&
      style_source.StyleRef().HasPseudoStyle(kPseudoIdScrollbar);

  Scrollbar* scrollbars[] = {HorizontalScrollbar(), VerticalScrollbar()};

  for (Scrollbar* scrollbar : scrollbars) {
    if (!scrollbar)
      continue;

    // We have a native scrollbar that should be custom, or vice versa.
    if (scrollbar->IsCustomScrollbar() != needs_custom)
      return true;

    if (needs_custom) {
      DCHECK(scrollbar->IsCustomScrollbar());
      // We have a custom scrollbar with a stale m_owner.
      if (ToLayoutScrollbar(scrollbar)->StyleSource() != style_source)
        return true;

      // Should use custom scrollbar and nothing should change.
      continue;
    }

    // Check if native scrollbar should change.
    Page* page = Box().GetFrame()->LocalFrameRoot().GetPage();
    DCHECK(page);
    ScrollbarTheme* current_theme = &page->GetScrollbarTheme();

    if (current_theme != &scrollbar->GetTheme())
      return true;
  }
  return false;
}

void PaintLayerScrollableArea::ComputeScrollbarExistence(
    bool& needs_horizontal_scrollbar,
    bool& needs_vertical_scrollbar,
    ComputeScrollbarExistenceOption option) const {
  // Scrollbars may be hidden or provided by visual viewport or frame instead.
  DCHECK(Box().GetFrame()->GetSettings());
  if (VisualViewportSuppliesScrollbars() || !CanHaveOverflowScrollbars(Box()) ||
      Box().GetFrame()->GetSettings()->GetHideScrollbars()) {
    needs_horizontal_scrollbar = false;
    needs_vertical_scrollbar = false;
    return;
  }

  needs_horizontal_scrollbar = Box().ScrollsOverflowX();
  needs_vertical_scrollbar = Box().ScrollsOverflowY();

  // Don't add auto scrollbars if the box contents aren't visible.
  if (Box().HasAutoHorizontalScrollbar()) {
    if (option == kForbidAddingAutoBars)
      needs_horizontal_scrollbar &= HasHorizontalScrollbar();
    needs_horizontal_scrollbar &=
        Box().IsRooted() && HasHorizontalOverflow() &&
        VisibleContentRect(kIncludeScrollbars).Height();
  }

  if (Box().HasAutoVerticalScrollbar()) {
    if (option == kForbidAddingAutoBars)
      needs_vertical_scrollbar &= HasVerticalScrollbar();
    needs_vertical_scrollbar &= Box().IsRooted() && HasVerticalOverflow() &&
                                VisibleContentRect(kIncludeScrollbars).Width();
  }

  if (Box().IsLayoutView()) {
    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(Box()).CalculateScrollbarModes(h_mode, v_mode);

    // Look for the scrollbarModes and reset the needs Horizontal & vertical
    // Scrollbar values based on scrollbarModes, as during force style change
    // StyleResolver::styleForDocument returns documentStyle with no overflow
    // values, due to which we are destroying the scrollbars that were already
    // present.
    if (h_mode == kScrollbarAlwaysOn)
      needs_horizontal_scrollbar = true;
    else if (h_mode == kScrollbarAlwaysOff)
      needs_horizontal_scrollbar = false;
    if (v_mode == kScrollbarAlwaysOn)
      needs_vertical_scrollbar = true;
    else if (v_mode == kScrollbarAlwaysOff)
      needs_vertical_scrollbar = false;
  }
}

bool PaintLayerScrollableArea::TryRemovingAutoScrollbars(
    const bool& needs_horizontal_scrollbar,
    const bool& needs_vertical_scrollbar) {
  // If scrollbars are removed but the content size depends on the scrollbars,
  // additional layouts will be required to size the content. Therefore, only
  // remove auto scrollbars for the initial layout pass.
  DCHECK(!in_overflow_relayout_);

  if (!needs_horizontal_scrollbar && !needs_vertical_scrollbar)
    return false;

  if (Box().IsLayoutView()) {
    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(Box()).CalculateScrollbarModes(h_mode, v_mode);
    if (h_mode != kScrollbarAuto || v_mode != kScrollbarAuto)
      return false;

    IntSize visible_size_with_scrollbars =
        VisibleContentRect(kIncludeScrollbars).Size();
    if (ScrollWidth() <= visible_size_with_scrollbars.Width() &&
        ScrollHeight() <= visible_size_with_scrollbars.Height()) {
      return true;
    }
  } else {
    if (!Box().HasAutoVerticalScrollbar() ||
        !Box().HasAutoHorizontalScrollbar())
      return false;

    LayoutSize client_size_with_scrollbars =
        LayoutContentRect(kIncludeScrollbars).Size();
    if (ScrollWidth() <= client_size_with_scrollbars.Width() &&
        ScrollHeight() <= client_size_with_scrollbars.Height()) {
      return true;
    }
  }

  return false;
}

bool PaintLayerScrollableArea::SetHasHorizontalScrollbar(bool has_scrollbar) {
  if (FreezeScrollbarsScope::ScrollbarsAreFrozen())
    return false;

  if (has_scrollbar == HasHorizontalScrollbar())
    return false;

  SetScrollbarNeedsPaintInvalidation(kHorizontalScrollbar);

  scrollbar_manager_.SetHasHorizontalScrollbar(has_scrollbar);

  UpdateScrollOrigin();

  // Destroying or creating one bar can cause our scrollbar corner to come and
  // go. We need to update the opposite scrollbar's style.
  if (HasHorizontalScrollbar())
    HorizontalScrollbar()->StyleChanged();
  if (HasVerticalScrollbar())
    VerticalScrollbar()->StyleChanged();

  SetScrollCornerNeedsPaintInvalidation();

  // Force an update since we know the scrollbars have changed things.
  if (Box().GetDocument().HasAnnotatedRegions())
    Box().GetDocument().SetAnnotatedRegionsDirty(true);
  return true;
}

bool PaintLayerScrollableArea::SetHasVerticalScrollbar(bool has_scrollbar) {
  if (FreezeScrollbarsScope::ScrollbarsAreFrozen())
    return false;

  if (has_scrollbar == HasVerticalScrollbar())
    return false;

  SetScrollbarNeedsPaintInvalidation(kVerticalScrollbar);

  scrollbar_manager_.SetHasVerticalScrollbar(has_scrollbar);

  UpdateScrollOrigin();

  // Destroying or creating one bar can cause our scrollbar corner to come and
  // go. We need to update the opposite scrollbar's style.
  if (HasHorizontalScrollbar())
    HorizontalScrollbar()->StyleChanged();
  if (HasVerticalScrollbar())
    VerticalScrollbar()->StyleChanged();

  SetScrollCornerNeedsPaintInvalidation();

  // Force an update since we know the scrollbars have changed things.
  if (Box().GetDocument().HasAnnotatedRegions())
    Box().GetDocument().SetAnnotatedRegionsDirty(true);
  return true;
}

int PaintLayerScrollableArea::VerticalScrollbarWidth(
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  if (!HasVerticalScrollbar())
    return 0;
  if (overlay_scrollbar_clip_behavior ==
          kIgnorePlatformAndCSSOverlayScrollbarSize &&
      Box().Style()->OverflowY() == EOverflow::kOverlay) {
    return 0;
  }
  if ((overlay_scrollbar_clip_behavior == kIgnorePlatformOverlayScrollbarSize ||
       overlay_scrollbar_clip_behavior ==
           kIgnorePlatformAndCSSOverlayScrollbarSize ||
       !VerticalScrollbar()->ShouldParticipateInHitTesting()) &&
      VerticalScrollbar()->IsOverlayScrollbar()) {
    return 0;
  }
  return VerticalScrollbar()->ScrollbarThickness();
}

int PaintLayerScrollableArea::HorizontalScrollbarHeight(
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  if (!HasHorizontalScrollbar())
    return 0;
  if (overlay_scrollbar_clip_behavior ==
          kIgnorePlatformAndCSSOverlayScrollbarSize &&
      Box().Style()->OverflowX() == EOverflow::kOverlay) {
    return 0;
  }
  if ((overlay_scrollbar_clip_behavior == kIgnorePlatformOverlayScrollbarSize ||
       overlay_scrollbar_clip_behavior ==
           kIgnorePlatformAndCSSOverlayScrollbarSize ||
       !HorizontalScrollbar()->ShouldParticipateInHitTesting()) &&
      HorizontalScrollbar()->IsOverlayScrollbar()) {
    return 0;
  }
  return HorizontalScrollbar()->ScrollbarThickness();
}

void PaintLayerScrollableArea::PositionOverflowControls() {
  if (!HasScrollbar() && !Box().CanResize())
    return;

  const IntRect border_box =
      Box().PixelSnappedBorderBoxRect(layer_.SubpixelAccumulation());

  if (Scrollbar* vertical_scrollbar = VerticalScrollbar())
    vertical_scrollbar->SetFrameRect(RectForVerticalScrollbar(border_box));

  if (Scrollbar* horizontal_scrollbar = HorizontalScrollbar())
    horizontal_scrollbar->SetFrameRect(RectForHorizontalScrollbar(border_box));

  const IntRect& scroll_corner = ScrollCornerRect();
  if (scroll_corner_)
    scroll_corner_->SetFrameRect(LayoutRect(scroll_corner));

  if (resizer_)
    resizer_->SetFrameRect(
        LayoutRect(ResizerCornerRect(border_box, kResizerForPointer)));

  // FIXME, this should eventually be removed, once we are certain that
  // composited controls get correctly positioned on a compositor update. For
  // now, conservatively leaving this unchanged.
  if (Layer()->HasCompositedLayerMapping())
    Layer()->GetCompositedLayerMapping()->PositionOverflowControlsLayers();
}

void PaintLayerScrollableArea::UpdateScrollCornerStyle() {
  if (!scroll_corner_ && !HasScrollbar())
    return;
  if (!scroll_corner_ && HasOverlayScrollbars())
    return;

  const LayoutObject& style_source = ScrollbarStyleSource(Box());
  scoped_refptr<ComputedStyle> corner =
      Box().HasOverflowClip()
          ? style_source.GetUncachedPseudoStyle(
                PseudoStyleRequest(kPseudoIdScrollbarCorner),
                style_source.Style())
          : scoped_refptr<ComputedStyle>(nullptr);
  if (corner) {
    if (!scroll_corner_) {
      scroll_corner_ =
          LayoutScrollbarPart::CreateAnonymous(&Box().GetDocument(), this);
      scroll_corner_->SetDangerousOneWayParent(&Box());
    }
    scroll_corner_->SetStyleWithWritingModeOfParent(std::move(corner));
  } else if (scroll_corner_) {
    scroll_corner_->Destroy();
    scroll_corner_ = nullptr;
  }
}

bool PaintLayerScrollableArea::HitTestOverflowControls(
    HitTestResult& result,
    const IntPoint& local_point) {
  if (!HasScrollbar() && !Box().CanResize())
    return false;

  IntRect resize_control_rect;
  if (Box().Style()->Resize() != EResize::kNone) {
    resize_control_rect = ResizerCornerRect(
        Box().PixelSnappedBorderBoxRect(Layer()->SubpixelAccumulation()),
        kResizerForPointer);
    if (resize_control_rect.Contains(local_point))
      return true;
  }

  int resize_control_size = max(resize_control_rect.Height(), 0);
  if (HasVerticalScrollbar() &&
      VerticalScrollbar()->ShouldParticipateInHitTesting()) {
    LayoutRect v_bar_rect(
        VerticalScrollbarStart(0, Layer()->PixelSnappedSize().Width()),
        Box().BorderTop().ToInt(), VerticalScrollbar()->ScrollbarThickness(),
        VisibleContentRect(kIncludeScrollbars).Height() -
            (HasHorizontalScrollbar()
                 ? HorizontalScrollbar()->ScrollbarThickness()
                 : resize_control_size));
    if (v_bar_rect.Contains(local_point)) {
      result.SetScrollbar(VerticalScrollbar());
      return true;
    }
  }

  resize_control_size = max(resize_control_rect.Width(), 0);
  if (HasHorizontalScrollbar() &&
      HorizontalScrollbar()->ShouldParticipateInHitTesting()) {
    // TODO(crbug.com/638981): Are the conversions to int intentional?
    LayoutRect h_bar_rect(
        HorizontalScrollbarStart(0),
        Box().BorderTop().ToInt() + VisibleHeight(),
        VisibleContentRect(kIncludeScrollbars).Width() -
            (HasVerticalScrollbar() ? VerticalScrollbar()->ScrollbarThickness()
                                    : resize_control_size),
        HorizontalScrollbar()->ScrollbarThickness());
    if (h_bar_rect.Contains(local_point)) {
      result.SetScrollbar(HorizontalScrollbar());
      return true;
    }
  }

  // FIXME: We should hit test the m_scrollCorner and pass it back through the
  // result.

  return false;
}

IntRect PaintLayerScrollableArea::ResizerCornerRect(
    const IntRect& bounds,
    ResizerHitTestType resizer_hit_test_type) const {
  if (Box().Style()->Resize() == EResize::kNone)
    return IntRect();
  IntRect corner = CornerRect(bounds);

  if (resizer_hit_test_type == kResizerForTouch) {
    // We make the resizer virtually larger for touch hit testing. With the
    // expanding ratio k = ResizerControlExpandRatioForTouch, we first move
    // the resizer rect (of width w & height h), by (-w * (k-1), -h * (k-1)),
    // then expand the rect by new_w/h = w/h * k.
    int expand_ratio = kResizerControlExpandRatioForTouch - 1;
    corner.Move(-corner.Width() * expand_ratio,
                -corner.Height() * expand_ratio);
    corner.Expand(corner.Width() * expand_ratio,
                  corner.Height() * expand_ratio);
  }

  return corner;
}

IntRect PaintLayerScrollableArea::ScrollCornerAndResizerRect() const {
  IntRect scroll_corner_and_resizer = ScrollCornerRect();
  if (scroll_corner_and_resizer.IsEmpty()) {
    scroll_corner_and_resizer = ResizerCornerRect(
        Box().PixelSnappedBorderBoxRect(Layer()->SubpixelAccumulation()),
        kResizerForPointer);
  }
  return scroll_corner_and_resizer;
}

bool PaintLayerScrollableArea::IsPointInResizeControl(
    const IntPoint& absolute_point,
    ResizerHitTestType resizer_hit_test_type) const {
  if (!Box().CanResize())
    return false;

  IntPoint local_point =
      RoundedIntPoint(Box().AbsoluteToLocal(absolute_point, kUseTransforms));
  IntRect local_bounds(IntPoint(), Layer()->PixelSnappedSize());
  return ResizerCornerRect(local_bounds, resizer_hit_test_type)
      .Contains(local_point);
}

bool PaintLayerScrollableArea::HitTestResizerInFragments(
    const PaintLayerFragments& layer_fragments,
    const HitTestLocation& hit_test_location) const {
  if (!Box().CanResize())
    return false;

  if (layer_fragments.IsEmpty())
    return false;

  for (int i = layer_fragments.size() - 1; i >= 0; --i) {
    const PaintLayerFragment& fragment = layer_fragments.at(i);
    if (fragment.background_rect.Intersects(hit_test_location) &&
        ResizerCornerRect(PixelSnappedIntRect(fragment.layer_bounds),
                          kResizerForPointer)
            .Contains(hit_test_location.RoundedPoint()))
      return true;
  }

  return false;
}

void PaintLayerScrollableArea::UpdateResizerAreaSet() {
  LocalFrame* frame = Box().GetFrame();
  if (!frame)
    return;
  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;
  if (Box().CanResize())
    frame_view->AddResizerArea(Box());
  else
    frame_view->RemoveResizerArea(Box());
}

void PaintLayerScrollableArea::UpdateResizerStyle(
    const ComputedStyle* old_style) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() && old_style &&
      old_style->Resize() != Box().StyleRef().Resize()) {
    // Invalidate the composited scroll corner layer on resize style change.
    if (auto* graphics_layer = LayerForScrollCorner())
      graphics_layer->SetNeedsDisplay();
  }

  if (!resizer_ && !Box().CanResize())
    return;

  const LayoutObject& style_source = ScrollbarStyleSource(Box());
  scoped_refptr<ComputedStyle> resizer =
      Box().HasOverflowClip()
          ? style_source.GetUncachedPseudoStyle(
                PseudoStyleRequest(kPseudoIdResizer), style_source.Style())
          : scoped_refptr<ComputedStyle>(nullptr);
  if (resizer) {
    if (!resizer_) {
      resizer_ =
          LayoutScrollbarPart::CreateAnonymous(&Box().GetDocument(), this);
      resizer_->SetDangerousOneWayParent(&Box());
    }
    resizer_->SetStyleWithWritingModeOfParent(std::move(resizer));
  } else if (resizer_) {
    resizer_->Destroy();
    resizer_ = nullptr;
  }
}

void PaintLayerScrollableArea::InvalidateAllStickyConstraints() {
  if (PaintLayerScrollableAreaRareData* d = RareData()) {
    for (PaintLayer* sticky_layer : d->sticky_constraints_map_.Keys()) {
      if (sticky_layer->GetLayoutObject().Style()->GetPosition() ==
          EPosition::kSticky)
        sticky_layer->SetNeedsCompositingInputsUpdate();
    }
    d->sticky_constraints_map_.clear();
  }
}

void PaintLayerScrollableArea::InvalidateStickyConstraintsFor(
    PaintLayer* layer,
    bool needs_compositing_update) {
  if (PaintLayerScrollableAreaRareData* d = RareData()) {
    d->sticky_constraints_map_.erase(layer);
    if (needs_compositing_update &&
        layer->GetLayoutObject().Style()->HasStickyConstrainedPosition())
      layer->SetNeedsCompositingInputsUpdate();
  }
}

bool PaintLayerScrollableArea::HasNonCompositedStickyDescendants() const {
  if (const PaintLayerScrollableAreaRareData* d = RareData()) {
    for (const PaintLayer* sticky_layer : d->sticky_constraints_map_.Keys()) {
      if (sticky_layer->GetLayoutObject().IsSlowRepaintConstrainedObject())
        return true;
    }
  }
  return false;
}

void PaintLayerScrollableArea::InvalidatePaintForStickyDescendants() {
  if (PaintLayerScrollableAreaRareData* d = RareData()) {
    for (PaintLayer* sticky_layer : d->sticky_constraints_map_.Keys()) {
      sticky_layer->GetLayoutObject()
          .SetShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
    }
  }
}

IntSize PaintLayerScrollableArea::OffsetFromResizeCorner(
    const IntPoint& absolute_point) const {
  // Currently the resize corner is either the bottom right corner or the bottom
  // left corner.
  // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be
  // the case?
  IntSize element_size = Layer()->PixelSnappedSize();
  if (Box().ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    element_size.SetWidth(0);
  IntPoint resizer_point = IntPoint(element_size);
  IntPoint local_point =
      RoundedIntPoint(Box().AbsoluteToLocal(absolute_point, kUseTransforms));
  return local_point - resizer_point;
}

LayoutSize PaintLayerScrollableArea::MinimumSizeForResizing(float zoom_factor) {
  LayoutUnit min_width = MinimumValueForLength(
      Box().StyleRef().MinWidth(), Box().ContainingBlock()->Size().Width());
  LayoutUnit min_height = MinimumValueForLength(
      Box().StyleRef().MinHeight(), Box().ContainingBlock()->Size().Height());
  min_width = std::max(LayoutUnit(min_width / zoom_factor),
                       LayoutUnit(kDefaultMinimumWidthForResizing));
  min_height = std::max(LayoutUnit(min_height / zoom_factor),
                        LayoutUnit(kDefaultMinimumHeightForResizing));
  return LayoutSize(min_width, min_height);
}

void PaintLayerScrollableArea::Resize(const IntPoint& pos,
                                      const LayoutSize& old_offset) {
  // FIXME: This should be possible on generated content but is not right now.
  if (!InResizeMode() || !Box().CanResize() || !Box().GetNode())
    return;

  DCHECK(Box().GetNode()->IsElementNode());
  Element* element = ToElement(Box().GetNode());

  Document& document = element->GetDocument();

  float zoom_factor = Box().Style()->EffectiveZoom();

  IntSize new_offset =
      OffsetFromResizeCorner(document.View()->RootFrameToContents(pos));
  new_offset.SetWidth(new_offset.Width() / zoom_factor);
  new_offset.SetHeight(new_offset.Height() / zoom_factor);

  LayoutSize current_size = Box().Size();
  current_size.Scale(1 / zoom_factor);

  LayoutSize adjusted_old_offset = LayoutSize(
      old_offset.Width() / zoom_factor, old_offset.Height() / zoom_factor);
  if (Box().ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    new_offset.SetWidth(-new_offset.Width());
    adjusted_old_offset.SetWidth(-adjusted_old_offset.Width());
  }

  LayoutSize difference((current_size + new_offset - adjusted_old_offset)
                            .ExpandedTo(MinimumSizeForResizing(zoom_factor)) -
                        current_size);

  bool is_box_sizing_border =
      Box().Style()->BoxSizing() == EBoxSizing::kBorderBox;

  EResize resize = Box().Style()->Resize();
  if (resize != EResize::kVertical && difference.Width()) {
    if (element->IsFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->SetInlineStyleProperty(CSSPropertyMarginLeft,
                                      Box().MarginLeft() / zoom_factor,
                                      CSSPrimitiveValue::UnitType::kPixels);
      element->SetInlineStyleProperty(CSSPropertyMarginRight,
                                      Box().MarginRight() / zoom_factor,
                                      CSSPrimitiveValue::UnitType::kPixels);
    }
    LayoutUnit base_width =
        Box().Size().Width() -
        (is_box_sizing_border ? LayoutUnit() : Box().BorderAndPaddingWidth());
    base_width = LayoutUnit(base_width / zoom_factor);
    element->SetInlineStyleProperty(CSSPropertyWidth,
                                    RoundToInt(base_width + difference.Width()),
                                    CSSPrimitiveValue::UnitType::kPixels);
  }

  if (resize != EResize::kHorizontal && difference.Height()) {
    if (element->IsFormControlElement()) {
      // Make implicit margins from the theme explicit (see
      // <http://bugs.webkit.org/show_bug.cgi?id=9547>).
      element->SetInlineStyleProperty(CSSPropertyMarginTop,
                                      Box().MarginTop() / zoom_factor,
                                      CSSPrimitiveValue::UnitType::kPixels);
      element->SetInlineStyleProperty(CSSPropertyMarginBottom,
                                      Box().MarginBottom() / zoom_factor,
                                      CSSPrimitiveValue::UnitType::kPixels);
    }
    LayoutUnit base_height =
        Box().Size().Height() -
        (is_box_sizing_border ? LayoutUnit() : Box().BorderAndPaddingHeight());
    base_height = LayoutUnit(base_height / zoom_factor);
    element->SetInlineStyleProperty(
        CSSPropertyHeight, RoundToInt(base_height + difference.Height()),
        CSSPrimitiveValue::UnitType::kPixels);
  }

  document.UpdateStyleAndLayout();

  // FIXME (Radar 4118564): We should also autoscroll the window as necessary to
  // keep the point under the cursor in view.
}

LayoutRect PaintLayerScrollableArea::ScrollIntoView(
    const LayoutRect& rect,
    const ScrollAlignment& align_x,
    const ScrollAlignment& align_y,
    bool is_smooth,
    ScrollType scroll_type,
    bool is_for_scroll_sequence) {
  LayoutRect local_expose_rect(
      Box()
          .AbsoluteToLocalQuad(FloatQuad(FloatRect(rect)), kUseTransforms)
          .BoundingBox());
  local_expose_rect.Move(-Box().BorderLeft(), -Box().BorderTop());
  LayoutRect visible_rect(IntPoint(), VisibleContentRect().Size());
  LayoutRect r = ScrollAlignment::GetRectToExpose(
      visible_rect, local_expose_rect, align_x, align_y);

  ScrollOffset old_scroll_offset = GetScrollOffset();
  ScrollOffset new_scroll_offset(ClampScrollOffset(RoundedIntSize(
      ToScrollOffset(FloatPoint(r.Location()) + old_scroll_offset))));
  if (is_for_scroll_sequence) {
    DCHECK(scroll_type == kProgrammaticScroll || scroll_type == kUserScroll);
    ScrollBehavior behavior =
        is_smooth ? kScrollBehaviorSmooth : kScrollBehaviorInstant;
    GetSmoothScrollSequencer()->QueueAnimation(this, new_scroll_offset,
                                               behavior);
  } else {
    SetScrollOffset(new_scroll_offset, scroll_type, kScrollBehaviorInstant);
  }
  ScrollOffset scroll_offset_difference =
      ClampScrollOffset(new_scroll_offset) - old_scroll_offset;
  local_expose_rect.Move(-LayoutSize(scroll_offset_difference));

  LayoutRect intersect =
      LocalToAbsolute(Box(), Intersection(visible_rect, local_expose_rect));
  if (intersect.IsEmpty() && !visible_rect.IsEmpty() &&
      !local_expose_rect.IsEmpty()) {
    return LocalToAbsolute(Box(), local_expose_rect);
  }
  return intersect;
}

void PaintLayerScrollableArea::UpdateScrollableAreaSet() {
  LocalFrame* frame = Box().GetFrame();
  if (!frame)
    return;

  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;

  bool has_overflow = !Box().Size().IsZero() &&
                      ((HasHorizontalOverflow() && Box().ScrollsOverflowX()) ||
                       (HasVerticalOverflow() && Box().ScrollsOverflowY()));

  bool is_visible_to_hit_test = Box().Style()->VisibleToHitTesting();
  bool did_scroll_overflow = scrolls_overflow_;
  if (Box().IsLayoutView()) {
    ScrollbarMode h_mode;
    ScrollbarMode v_mode;
    ToLayoutView(Box()).CalculateScrollbarModes(h_mode, v_mode);
    if (h_mode == kScrollbarAlwaysOff && v_mode == kScrollbarAlwaysOff)
      has_overflow = false;
  }
  scrolls_overflow_ = has_overflow && is_visible_to_hit_test;
  if (did_scroll_overflow == ScrollsOverflow())
    return;

  // The scroll and scroll offset properties depend on |scrollsOverflow| (see:
  // PaintPropertyTreeBuilder::updateScrollAndScrollTranslation).
  Box().SetNeedsPaintPropertyUpdate();

  if (scrolls_overflow_) {
    DCHECK(CanHaveOverflowScrollbars(Box()));
    frame_view->AddScrollableArea(this);
  } else {
    frame_view->RemoveScrollableArea(this);
  }

  layer_.DidUpdateScrollsOverflow();
}

void PaintLayerScrollableArea::UpdateCompositingLayersAfterScroll() {
  PaintLayerCompositor* compositor = Box().View()->Compositor();
  if (!compositor->InCompositingMode())
    return;

  if (UsesCompositedScrolling()) {
    DCHECK(Layer()->HasCompositedLayerMapping());
    Layer()->GetCompositedLayerMapping()->SetNeedsGraphicsLayerUpdate(
        kGraphicsLayerUpdateSubtree);
    compositor->SetNeedsCompositingUpdate(
        kCompositingUpdateAfterGeometryChange);

    // If we have fixed elements and we scroll the root layer in RLS we might
    // change compositing since the fixed elements might now overlap a
    // composited layer.
    if (Layer()->IsRootLayer() &&
        RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      LocalFrame* frame = Box().GetFrame();
      if (frame && frame->View() &&
          frame->View()->HasViewportConstrainedObjects()) {
        Layer()->SetNeedsCompositingInputsUpdate();
      }
    }
  } else {
    Layer()->SetNeedsCompositingInputsUpdate();
  }
}

ScrollingCoordinator* PaintLayerScrollableArea::GetScrollingCoordinator()
    const {
  LocalFrame* frame = Box().GetFrame();
  if (!frame)
    return nullptr;

  Page* page = frame->GetPage();
  if (!page)
    return nullptr;

  return page->GetScrollingCoordinator();
}

bool PaintLayerScrollableArea::UsesCompositedScrolling() const {
  // See https://codereview.chromium.org/176633003/ for the tests that fail
  // without this disabler.
  DisableCompositingQueryAsserts disabler;
  return Layer()->HasCompositedLayerMapping() &&
         Layer()->GetCompositedLayerMapping()->ScrollingLayer();
}

bool PaintLayerScrollableArea::ShouldScrollOnMainThread() const {
  if (LocalFrame* frame = Box().GetFrame()) {
    if (frame->View()->GetMainThreadScrollingReasons())
      return true;
  }
  if (HasNonCompositedStickyDescendants())
    return true;
  return ScrollableArea::ShouldScrollOnMainThread();
}

static bool LayerNodeMayNeedCompositedScrolling(const PaintLayer* layer) {
  // Don't force composite scroll for select or text input elements.
  if (Node* node = layer->GetLayoutObject().GetNode()) {
    if (IsHTMLSelectElement(node))
      return false;
    if (TextControlElement* text_control = EnclosingTextControl(node)) {
      if (IsHTMLInputElement(text_control)) {
        return false;
      }
    }
  }
  return true;
}

bool PaintLayerScrollableArea::ComputeNeedsCompositedScrolling(
    const bool layer_has_been_composited,
    const PaintLayer* layer) {
  non_composited_main_thread_scrolling_reasons_ = 0;

  // The root scroller needs composited scrolling layers even if it doesn't
  // actually have scrolling since CC has these assumptions baked in for the
  // viewport. If we're in non-RootLayerScrolling mode, the root layer will be
  // the global root scroller (by default) but it doesn't actually handle
  // scrolls itself so we don't need composited scrolling for it.
  if (RootScrollerUtil::IsGlobal(*layer) && !Layer()->IsScrolledByFrameView())
    return true;

  if (!layer->ScrollsOverflow())
    return false;

  if (layer->Size().IsEmpty())
    return false;

  if (!layer_has_been_composited &&
      !LayerNodeMayNeedCompositedScrolling(layer)) {
    return false;
  }

  bool needs_composited_scrolling = true;

  // TODO(flackr): Allow integer transforms as long as all of the ancestor
  // transforms are also integer.
  bool background_supports_lcd_text =
      RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled() &&
      layer->GetLayoutObject().Style()->IsStackingContext() &&
      layer->GetBackgroundPaintLocation(
          &non_composited_main_thread_scrolling_reasons_) &
          kBackgroundPaintInScrollingContents &&
      layer->BackgroundIsKnownToBeOpaqueInRect(
          ToLayoutBox(layer->GetLayoutObject()).PaddingBoxRect()) &&
      !layer->CompositesWithTransform() && !layer->CompositesWithOpacity();

  if (!layer_has_been_composited &&
      !layer->Compositor()->PreferCompositingToLCDTextEnabled() &&
      !background_supports_lcd_text) {
    if (layer->CompositesWithOpacity()) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kHasOpacityAndLCDText;
    }
    if (layer->CompositesWithTransform()) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kHasTransformAndLCDText;
    }
    if (!layer->BackgroundIsKnownToBeOpaqueInRect(
            ToLayoutBox(layer->GetLayoutObject()).PaddingBoxRect())) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText;
    }
    if (!layer->GetLayoutObject().Style()->IsStackingContext()) {
      non_composited_main_thread_scrolling_reasons_ |=
          MainThreadScrollingReason::kIsNotStackingContextAndLCDText;
    }

    needs_composited_scrolling = false;
  }

  if (layer->GetLayoutObject().Style()->HasBorderRadius()) {
    non_composited_main_thread_scrolling_reasons_ |=
        MainThreadScrollingReason::kHasBorderRadius;
    needs_composited_scrolling = false;
  }
  if (layer->GetLayoutObject().HasClip() ||
      layer->HasDescendantWithClipPath() || layer->HasAncestorWithClipPath()) {
    non_composited_main_thread_scrolling_reasons_ |=
        MainThreadScrollingReason::kHasClipRelatedProperty;
    needs_composited_scrolling = false;
  }

  DCHECK(!(non_composited_main_thread_scrolling_reasons_ &
           ~MainThreadScrollingReason::kNonCompositedReasons));
  return needs_composited_scrolling;
}

void PaintLayerScrollableArea::UpdateNeedsCompositedScrolling(
    bool layer_has_been_composited) {
  const bool needs_composited_scrolling =
      ComputeNeedsCompositedScrolling(layer_has_been_composited, Layer());

  if (static_cast<bool>(needs_composited_scrolling_) !=
      needs_composited_scrolling) {
    needs_composited_scrolling_ = needs_composited_scrolling;
  }
}

void PaintLayerScrollableArea::SetTopmostScrollChild(PaintLayer* scroll_child) {
  // We only want to track the topmost scroll child for scrollable areas with
  // overlay scrollbars.
  if (!HasOverlayScrollbars())
    return;
  next_topmost_scroll_child_ = scroll_child;
}

bool PaintLayerScrollableArea::VisualViewportSuppliesScrollbars() const {
  LocalFrame* frame = Box().GetFrame();
  if (!frame || !frame->GetSettings())
    return false;

  // On desktop, we always use the layout viewport's scrollbars.
  if (!frame->GetSettings()->GetViewportEnabled())
    return false;

  const TopDocumentRootScrollerController& controller =
      GetLayoutBox()->GetDocument().GetPage()->GlobalRootScrollerController();

  return RootScrollerUtil::ScrollableAreaForRootScroller(
             controller.GlobalRootScroller()) == this;
}

bool PaintLayerScrollableArea::ScheduleAnimation() {
  if (PlatformChromeClient* client = GetChromeClient()) {
    client->ScheduleAnimation(Box().GetFrame()->View());
    return true;
  }
  return false;
}

void PaintLayerScrollableArea::ResetRebuildScrollbarLayerFlags() {
  rebuild_horizontal_scrollbar_layer_ = false;
  rebuild_vertical_scrollbar_layer_ = false;
}

CompositorAnimationHost* PaintLayerScrollableArea::GetCompositorAnimationHost()
    const {
  return layer_.GetLayoutObject().GetFrameView()->GetCompositorAnimationHost();
}

CompositorAnimationTimeline*
PaintLayerScrollableArea::GetCompositorAnimationTimeline() const {
  return layer_.GetLayoutObject()
      .GetFrameView()
      ->GetCompositorAnimationTimeline();
}

void PaintLayerScrollableArea::GetTickmarks(Vector<IntRect>& tickmarks) const {
  if (layer_.IsRootLayer())
    tickmarks = Box().GetDocument().Markers().LayoutRectsForTextMatchMarkers();
}

PaintLayerScrollableArea*
PaintLayerScrollableArea::ScrollbarManager::ScrollableArea() {
  return ToPaintLayerScrollableArea(scrollable_area_.Get());
}

void PaintLayerScrollableArea::ScrollbarManager::DestroyDetachedScrollbars() {
  DCHECK(!h_bar_is_attached_ || h_bar_);
  DCHECK(!v_bar_is_attached_ || v_bar_);
  if (h_bar_ && !h_bar_is_attached_)
    DestroyScrollbar(kHorizontalScrollbar);
  if (v_bar_ && !v_bar_is_attached_)
    DestroyScrollbar(kVerticalScrollbar);
}

void PaintLayerScrollableArea::ScrollbarManager::SetHasHorizontalScrollbar(
    bool has_scrollbar) {
  if (has_scrollbar) {
    // This doesn't hit in any tests, but since the equivalent code in
    // setHasVerticalScrollbar does, presumably this code does as well.
    DisableCompositingQueryAsserts disabler;
    if (!h_bar_) {
      h_bar_ = CreateScrollbar(kHorizontalScrollbar);
      h_bar_is_attached_ = 1;
      if (!h_bar_->IsCustomScrollbar())
        ScrollableArea()->DidAddScrollbar(*h_bar_, kHorizontalScrollbar);
    } else {
      h_bar_is_attached_ = 1;
    }
  } else {
    h_bar_is_attached_ = 0;
    if (!DelayScrollOffsetClampScope::ClampingIsDelayed())
      DestroyScrollbar(kHorizontalScrollbar);
  }
}

void PaintLayerScrollableArea::ScrollbarManager::SetHasVerticalScrollbar(
    bool has_scrollbar) {
  if (has_scrollbar) {
    DisableCompositingQueryAsserts disabler;
    if (!v_bar_) {
      v_bar_ = CreateScrollbar(kVerticalScrollbar);
      v_bar_is_attached_ = 1;
      if (!v_bar_->IsCustomScrollbar())
        ScrollableArea()->DidAddScrollbar(*v_bar_, kVerticalScrollbar);
    } else {
      v_bar_is_attached_ = 1;
    }
  } else {
    v_bar_is_attached_ = 0;
    if (!DelayScrollOffsetClampScope::ClampingIsDelayed())
      DestroyScrollbar(kVerticalScrollbar);
  }
}

Scrollbar* PaintLayerScrollableArea::ScrollbarManager::CreateScrollbar(
    ScrollbarOrientation orientation) {
  DCHECK(orientation == kHorizontalScrollbar ? !h_bar_is_attached_
                                             : !v_bar_is_attached_);
  Scrollbar* scrollbar = nullptr;
  const LayoutObject& style_source =
      ScrollbarStyleSource(ScrollableArea()->Box());
  bool has_custom_scrollbar_style =
      style_source.IsBox() &&
      style_source.StyleRef().HasPseudoStyle(kPseudoIdScrollbar);
  if (has_custom_scrollbar_style) {
    DCHECK(style_source.GetNode() && style_source.GetNode()->IsElementNode());
    scrollbar = LayoutScrollbar::CreateCustomScrollbar(
        ScrollableArea(), orientation, ToElement(style_source.GetNode()));
  } else {
    ScrollbarControlSize scrollbar_size = kRegularScrollbar;
    if (style_source.StyleRef().HasAppearance()) {
      scrollbar_size = LayoutTheme::GetTheme().ScrollbarControlSizeForPart(
          style_source.StyleRef().Appearance());
    }
    scrollbar = Scrollbar::Create(
        ScrollableArea(), orientation, scrollbar_size,
        &ScrollableArea()->Box().GetFrame()->GetPage()->GetChromeClient());
  }
  ScrollableArea()->Box().GetDocument().View()->AddScrollbar(scrollbar);
  return scrollbar;
}

void PaintLayerScrollableArea::ScrollbarManager::DestroyScrollbar(
    ScrollbarOrientation orientation) {
  Member<Scrollbar>& scrollbar =
      orientation == kHorizontalScrollbar ? h_bar_ : v_bar_;
  DCHECK(orientation == kHorizontalScrollbar ? !h_bar_is_attached_
                                             : !v_bar_is_attached_);
  if (!scrollbar)
    return;

  ScrollableArea()->SetScrollbarNeedsPaintInvalidation(orientation);
  if (orientation == kHorizontalScrollbar)
    ScrollableArea()->rebuild_horizontal_scrollbar_layer_ = true;
  else
    ScrollableArea()->rebuild_vertical_scrollbar_layer_ = true;

  if (!scrollbar->IsCustomScrollbar())
    ScrollableArea()->WillRemoveScrollbar(*scrollbar, orientation);

  ScrollableArea()->Box().GetDocument().View()->RemoveScrollbar(scrollbar);
  scrollbar->DisconnectFromScrollableArea();
  scrollbar = nullptr;
}

uint64_t PaintLayerScrollableArea::Id() const {
  return DOMNodeIds::IdForNode(Box().GetNode());
}

int PaintLayerScrollableArea::PreventRelayoutScope::count_ = 0;
SubtreeLayoutScope*
    PaintLayerScrollableArea::PreventRelayoutScope::layout_scope_ = nullptr;
bool PaintLayerScrollableArea::PreventRelayoutScope::relayout_needed_ = false;
PersistentHeapVector<Member<PaintLayerScrollableArea>>*
    PaintLayerScrollableArea::PreventRelayoutScope::needs_relayout_ = nullptr;

PaintLayerScrollableArea::PreventRelayoutScope::PreventRelayoutScope(
    SubtreeLayoutScope& layout_scope) {
  if (!count_) {
    DCHECK(!layout_scope_);
    DCHECK(!needs_relayout_ || needs_relayout_->IsEmpty());
    layout_scope_ = &layout_scope;
  }
  count_++;
}

PaintLayerScrollableArea::PreventRelayoutScope::~PreventRelayoutScope() {
  if (--count_ == 0) {
    if (relayout_needed_) {
      for (auto scrollable_area : *needs_relayout_) {
        DCHECK(scrollable_area->NeedsRelayout());
        LayoutBox& box = scrollable_area->Box();
        layout_scope_->SetNeedsLayout(
            &box, LayoutInvalidationReason::kScrollbarChanged);
        if (box.IsLayoutBlock()) {
          bool horizontal_scrollbar_changed =
              scrollable_area->HasHorizontalScrollbar() !=
              scrollable_area->HadHorizontalScrollbarBeforeRelayout();
          bool vertical_scrollbar_changed =
              scrollable_area->HasVerticalScrollbar() !=
              scrollable_area->HadVerticalScrollbarBeforeRelayout();
          if (horizontal_scrollbar_changed || vertical_scrollbar_changed)
            ToLayoutBlock(box).ScrollbarsChanged(horizontal_scrollbar_changed,
                                                 vertical_scrollbar_changed);
        }
        scrollable_area->SetNeedsRelayout(false);
      }

      needs_relayout_->clear();
    }
    layout_scope_ = nullptr;
  }
}

void PaintLayerScrollableArea::PreventRelayoutScope::SetBoxNeedsLayout(
    PaintLayerScrollableArea& scrollable_area,
    bool had_horizontal_scrollbar,
    bool had_vertical_scrollbar) {
  DCHECK(count_);
  DCHECK(layout_scope_);
  if (scrollable_area.NeedsRelayout())
    return;
  scrollable_area.SetNeedsRelayout(true);
  scrollable_area.SetHadHorizontalScrollbarBeforeRelayout(
      had_horizontal_scrollbar);
  scrollable_area.SetHadVerticalScrollbarBeforeRelayout(had_vertical_scrollbar);

  relayout_needed_ = true;
  if (!needs_relayout_)
    needs_relayout_ =
        new PersistentHeapVector<Member<PaintLayerScrollableArea>>();
  needs_relayout_->push_back(&scrollable_area);
}

void PaintLayerScrollableArea::PreventRelayoutScope::ResetRelayoutNeeded() {
  DCHECK_EQ(count_, 0);
  DCHECK(!needs_relayout_ || needs_relayout_->IsEmpty());
  relayout_needed_ = false;
}

int PaintLayerScrollableArea::FreezeScrollbarsScope::count_ = 0;

int PaintLayerScrollableArea::DelayScrollOffsetClampScope::count_ = 0;
PersistentHeapVector<Member<PaintLayerScrollableArea>>*
    PaintLayerScrollableArea::DelayScrollOffsetClampScope::needs_clamp_ =
        nullptr;

PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    DelayScrollOffsetClampScope() {
  if (!needs_clamp_)
    needs_clamp_ = new PersistentHeapVector<Member<PaintLayerScrollableArea>>();
  DCHECK(count_ > 0 || needs_clamp_->IsEmpty());
  count_++;
}

PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    ~DelayScrollOffsetClampScope() {
  if (--count_ == 0)
    DelayScrollOffsetClampScope::ClampScrollableAreas();
}

void PaintLayerScrollableArea::DelayScrollOffsetClampScope::SetNeedsClamp(
    PaintLayerScrollableArea* scrollable_area) {
  if (!scrollable_area->NeedsScrollOffsetClamp()) {
    scrollable_area->SetNeedsScrollOffsetClamp(true);
    needs_clamp_->push_back(scrollable_area);
  }
}

void PaintLayerScrollableArea::DelayScrollOffsetClampScope::
    ClampScrollableAreas() {
  for (auto& scrollable_area : *needs_clamp_)
    scrollable_area->ClampScrollOffsetAfterOverflowChange();
  delete needs_clamp_;
  needs_clamp_ = nullptr;
}

ScrollbarTheme& PaintLayerScrollableArea::GetPageScrollbarTheme() const {
  Page* page = Box().GetFrame()->GetPage();
  DCHECK(page);

  return page->GetScrollbarTheme();
}

}  // namespace blink
