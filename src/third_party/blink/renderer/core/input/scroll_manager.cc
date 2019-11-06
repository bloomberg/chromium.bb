// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/scroll_manager.h"

#include <utility>

#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_customization.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {
namespace {

cc::SnapFlingController::GestureScrollType ToGestureScrollType(
    WebInputEvent::Type web_event_type) {
  switch (web_event_type) {
    case WebInputEvent::kGestureScrollBegin:
      return cc::SnapFlingController::GestureScrollType::kBegin;
    case WebInputEvent::kGestureScrollUpdate:
      return cc::SnapFlingController::GestureScrollType::kUpdate;
    case WebInputEvent::kGestureScrollEnd:
      return cc::SnapFlingController::GestureScrollType::kEnd;
    default:
      NOTREACHED();
      return cc::SnapFlingController::GestureScrollType::kBegin;
  }
}

cc::SnapFlingController::GestureScrollUpdateInfo GetGestureScrollUpdateInfo(
    const WebGestureEvent& event) {
  return {
      .delta = gfx::Vector2dF(-event.data.scroll_update.delta_x,
                              -event.data.scroll_update.delta_y),
      .is_in_inertial_phase = event.data.scroll_update.inertial_phase ==
                              WebGestureEvent::InertialPhaseState::kMomentum,
      .event_time = event.TimeStamp()};
}

ScrollableArea* ScrollableAreaForSnapping(LayoutBox* layout_box) {
  if (!layout_box)
    return nullptr;

  return layout_box->IsLayoutView()
             ? layout_box->GetFrameView()->GetScrollableArea()
             : layout_box->GetScrollableArea();
}

}  // namespace

ScrollManager::ScrollManager(LocalFrame& frame) : frame_(frame) {
  snap_fling_controller_ = std::make_unique<cc::SnapFlingController>(this);

  Clear();
}

void ScrollManager::Clear() {
  scrollbar_handling_scroll_gesture_ = nullptr;
  resize_scrollable_area_ = nullptr;
  offset_from_resize_corner_ = LayoutSize();
  ClearGestureScrollState();
}

void ScrollManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(scroll_gesture_handling_node_);
  visitor->Trace(previous_gesture_scrolled_node_);
  visitor->Trace(scrollbar_handling_scroll_gesture_);
  visitor->Trace(resize_scrollable_area_);
  visitor->Trace(last_logical_scrolled_node_);
}

void ScrollManager::ClearGestureScrollState() {
  last_gesture_scroll_over_embedded_content_view_ = false;
  scroll_gesture_handling_node_ = nullptr;
  previous_gesture_scrolled_node_ = nullptr;
  delta_consumed_for_scroll_sequence_ = false;
  did_scroll_x_for_scroll_gesture_ = false;
  did_scroll_y_for_scroll_gesture_ = false;
  current_scroll_chain_.clear();

  if (Page* page = frame_->GetPage()) {
    bool reset_x = true;
    bool reset_y = true;
    page->GetOverscrollController().ResetAccumulated(reset_x, reset_y);
  }
}

void ScrollManager::StopAutoscroll() {
  if (AutoscrollController* controller = GetAutoscrollController())
    controller->StopAutoscroll();
}

void ScrollManager::StopMiddleClickAutoscroll() {
  if (AutoscrollController* controller = GetAutoscrollController())
    controller->StopMiddleClickAutoscroll(frame_);
}

bool ScrollManager::MiddleClickAutoscrollInProgress() const {
  return GetAutoscrollController() &&
         GetAutoscrollController()->MiddleClickAutoscrollInProgress();
}

AutoscrollController* ScrollManager::GetAutoscrollController() const {
  if (Page* page = frame_->GetPage())
    return &page->GetAutoscrollController();
  return nullptr;
}

static bool CanPropagate(const ScrollState& scroll_state, const Node& node) {
  ScrollableArea* scrollable_area = node.GetLayoutBox()->GetScrollableArea();
  if (!scrollable_area)
    return true;

  if (!scrollable_area->UserInputScrollable(kHorizontalScrollbar) &&
      !scrollable_area->UserInputScrollable(kVerticalScrollbar))
    return true;

  return (scroll_state.deltaXHint() == 0 ||
          node.GetComputedStyle()->OverscrollBehaviorX() ==
              EOverscrollBehavior::kAuto) &&
         (scroll_state.deltaYHint() == 0 ||
          node.GetComputedStyle()->OverscrollBehaviorY() ==
              EOverscrollBehavior::kAuto);
}

void ScrollManager::RecomputeScrollChain(const Node& start_node,
                                         const ScrollState& scroll_state,
                                         std::deque<DOMNodeId>& scroll_chain) {
  DCHECK(scroll_chain.empty());
  scroll_chain.clear();

  DCHECK(start_node.GetLayoutObject());
  LayoutBox* cur_box = start_node.GetLayoutObject()->EnclosingBox();

  // Scrolling propagates along the containing block chain and ends at the
  // RootScroller node. The RootScroller node will have a custom applyScroll
  // callback that performs scrolling as well as associated "root" actions like
  // browser control movement and overscroll glow.
  while (cur_box) {
    Node* cur_node = cur_box->GetNode();

    if (cur_node) {
      if (CanScroll(scroll_state, *cur_node))
        scroll_chain.push_front(DOMNodeIds::IdForNode(cur_node));

      if (cur_node->IsEffectiveRootScroller())
        break;

      if (!CanPropagate(scroll_state, *cur_node)) {
        // We should add the first node with non-auto overscroll-behavior to
        // the scroll chain regardlessly, as it's the only node we can latch to.
        if (scroll_chain.empty() ||
            scroll_chain.front() != DOMNodeIds::IdForNode(cur_node)) {
          scroll_chain.push_front(DOMNodeIds::IdForNode(cur_node));
        }
        break;
      }
    }

    cur_box = cur_box->ContainingBlock();
  }
}

bool ScrollManager::CanScroll(const ScrollState& scroll_state,
                              const Node& current_node) {
  if (!current_node.GetLayoutBox())
    return false;

  // We need to always add the global root scroller even if it isn't scrollable
  // since we can always pinch-zoom and scroll as well as for overscroll
  // effects.
  if (current_node.GetLayoutBox()->IsGlobalRootScroller())
    return true;

  ScrollableArea* scrollable_area =
      current_node.GetLayoutBox()->GetScrollableArea();

  if (!scrollable_area)
    return false;

  double delta_x = scroll_state.isBeginning() ? scroll_state.deltaXHint()
                                              : scroll_state.deltaX();
  double delta_y = scroll_state.isBeginning() ? scroll_state.deltaYHint()
                                              : scroll_state.deltaY();
  if (!delta_x && !delta_y)
    return true;

  if (!scrollable_area->UserInputScrollable(kHorizontalScrollbar))
    delta_x = 0;
  if (!scrollable_area->UserInputScrollable(kVerticalScrollbar))
    delta_y = 0;

  ScrollOffset current_offset = scrollable_area->GetScrollOffset();
  ScrollOffset target_offset = current_offset + ScrollOffset(delta_x, delta_y);
  ScrollOffset clamped_offset =
      scrollable_area->ClampScrollOffset(target_offset);
  return clamped_offset != current_offset;
}

bool ScrollManager::LogicalScroll(ScrollDirection direction,
                                  ScrollGranularity granularity,
                                  Node* start_node,
                                  Node* mouse_press_node) {
  Node* node = start_node;

  if (!node)
    node = frame_->GetDocument()->FocusedElement();

  if (!node)
    node = mouse_press_node;

  if ((!node || !node->GetLayoutObject()) &&
      (last_logical_scrolled_node_ &&
       last_logical_scrolled_node_->GetLayoutObject()))
    node = last_logical_scrolled_node_;

  if ((!node || !node->GetLayoutObject()) && frame_->View() &&
      frame_->View()->GetLayoutView())
    node = frame_->View()->GetLayoutView()->GetNode();

  if (!node)
    return false;

  Document& document = node->GetDocument();

  document.UpdateStyleAndLayout();

  std::deque<DOMNodeId> scroll_chain;
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  ScrollState* scroll_state = ScrollState::Create(std::move(scroll_state_data));
  RecomputeScrollChain(*node, *scroll_state, scroll_chain);

  while (!scroll_chain.empty()) {
    Node* scroll_chain_node = DOMNodeIds::NodeForId(scroll_chain.back());
    scroll_chain.pop_back();
    DCHECK(scroll_chain_node);

    LayoutBox* box = ToLayoutBox(scroll_chain_node->GetLayoutObject());
    DCHECK(box);

    ScrollDirectionPhysical physical_direction =
        ToPhysicalDirection(direction, box->IsHorizontalWritingMode(),
                            box->Style()->IsFlippedBlocksWritingMode());

    ScrollableArea* scrollable_area = nullptr;

    // The global root scroller must be scrolled by the RootFrameViewport.
    if (box->IsGlobalRootScroller()) {
      LocalFrame& main_frame = frame_->LocalFrameRoot();
      // The local root must be the main frame if we have the global root
      // scroller since it can't yet be set on OOPIFs.
      DCHECK(main_frame.IsMainFrame());

      scrollable_area = main_frame.View()->GetScrollableArea();
    } else {
      scrollable_area = box->GetScrollableArea();
    }

    DCHECK(scrollable_area);

    SnapCoordinator* snap_coordinator =
        frame_->GetDocument()->GetSnapCoordinator();
    ScrollOffset delta = ToScrollDelta(physical_direction, 1);
    delta.Scale(scrollable_area->ScrollStep(granularity, kHorizontalScrollbar),
                scrollable_area->ScrollStep(granularity, kVerticalScrollbar));
    // Pressing the arrow key is considered as a scroll with intended direction
    // only. Pressing the PgUp/PgDn key is considered as a scroll with intended
    // direction and end position. Pressing the Home/End key is considered as a
    // scroll with intended end position only.
    switch (granularity) {
      case ScrollGranularity::kScrollByLine: {
        if (snap_coordinator->SnapForDirection(*box, delta))
          return true;
        break;
      }
      case ScrollGranularity::kScrollByPage: {
        if (snap_coordinator->SnapForEndAndDirection(*box, delta))
          return true;
        break;
      }
      case ScrollGranularity::kScrollByDocument: {
        FloatPoint end_position = scrollable_area->ScrollPosition() + delta;
        bool scrolled_x = physical_direction == kScrollLeft ||
                          physical_direction == kScrollRight;
        bool scrolled_y = physical_direction == kScrollUp ||
                          physical_direction == kScrollDown;
        if (snap_coordinator->SnapForEndPosition(*box, end_position, scrolled_x,
                                                 scrolled_y))
          return true;
        break;
      }
      default:
        NOTREACHED();
    }

    ScrollResult result = scrollable_area->UserScroll(
        granularity, ToScrollDelta(physical_direction, 1));

    if (result.DidScroll()) {
      last_logical_scrolled_node_ = scroll_chain_node;
      return true;
    }
  }

  return false;
}

// TODO(bokan): This should be merged with logicalScroll assuming
// defaultSpaceEventHandler's chaining scroll can be done crossing frames.
bool ScrollManager::BubblingScroll(ScrollDirection direction,
                                   ScrollGranularity granularity,
                                   Node* starting_node,
                                   Node* mouse_press_node) {
  // The layout needs to be up to date to determine if we can scroll. We may be
  // here because of an onLoad event, in which case the final layout hasn't been
  // performed yet.
  frame_->GetDocument()->UpdateStyleAndLayout();
  // FIXME: enable scroll customization in this case. See crbug.com/410974.
  if (LogicalScroll(direction, granularity, starting_node, mouse_press_node))
    return true;

  Frame* parent_frame = frame_->Tree().Parent();
  if (!parent_frame)
    return false;

  return parent_frame->BubbleLogicalScrollFromChildFrame(direction, granularity,
                                                         frame_);
}

void ScrollManager::CustomizedScroll(ScrollState& scroll_state) {
  TRACE_EVENT0("input", "ScrollManager::CustomizedScroll");
  if (scroll_state.FullyConsumed())
    return;

  if (scroll_state.deltaX() || scroll_state.deltaY())
    frame_->GetDocument()->UpdateStyleAndLayout();

  DCHECK(!current_scroll_chain_.empty());

  scroll_state.SetScrollChain(current_scroll_chain_);

  scroll_state.distributeToScrollChainDescendant();
}

void ScrollManager::ComputeScrollRelatedMetrics(
    uint32_t* non_composited_main_thread_scrolling_reasons) {
  // When scrolling on the main thread, the scrollableArea may or may not be
  // composited. Either way, we have recorded either the reasons stored in
  // its layer or the reason NonFastScrollableRegion from the compositor
  // side. Here we record scrolls that occurred on main thread due to a
  // non-composited scroller.
  if (!scroll_gesture_handling_node_->GetLayoutObject())
    return;

  for (auto* cur_box =
           scroll_gesture_handling_node_->GetLayoutObject()->EnclosingBox();
       cur_box; cur_box = cur_box->ContainingBlock()) {
    PaintLayerScrollableArea* scrollable_area = cur_box->GetScrollableArea();

    if (!scrollable_area || !scrollable_area->ScrollsOverflow())
      continue;

    // TODO(bokan): This DCHECK is occasionally tripped. See crbug.com/944706.
    // DCHECK(!scrollable_area->UsesCompositedScrolling() ||
    //        !scrollable_area->GetNonCompositedMainThreadScrollingReasons());
    *non_composited_main_thread_scrolling_reasons |=
        scrollable_area->GetNonCompositedMainThreadScrollingReasons();
  }
}

void ScrollManager::RecordScrollRelatedMetrics(const WebGestureDevice device) {
  if (device != WebGestureDevice::kTouchpad &&
      device != WebGestureDevice::kTouchscreen) {
    return;
  }

  uint32_t non_composited_main_thread_scrolling_reasons = 0;
  ComputeScrollRelatedMetrics(&non_composited_main_thread_scrolling_reasons);

  if (non_composited_main_thread_scrolling_reasons) {
    DCHECK(cc::MainThreadScrollingReason::HasNonCompositedScrollReasons(
        non_composited_main_thread_scrolling_reasons));
    uint32_t main_thread_scrolling_reason_enum_max =
        cc::MainThreadScrollingReason::kMainThreadScrollingReasonCount + 1;
    for (uint32_t i = cc::MainThreadScrollingReason::kNonCompositedReasonsFirst;
         i <= cc::MainThreadScrollingReason::kNonCompositedReasonsLast; ++i) {
      unsigned val = 1 << i;
      if (non_composited_main_thread_scrolling_reasons & val) {
        if (device == WebGestureDevice::kTouchscreen) {
          DEFINE_STATIC_LOCAL(EnumerationHistogram, touch_histogram,
                              ("Renderer4.MainThreadGestureScrollReason",
                               main_thread_scrolling_reason_enum_max));
          touch_histogram.Count(i + 1);
        } else {
          DEFINE_STATIC_LOCAL(EnumerationHistogram, wheel_histogram,
                              ("Renderer4.MainThreadWheelScrollReason",
                               main_thread_scrolling_reason_enum_max));
          wheel_histogram.Count(i + 1);
        }
      }
    }
  }
}

WebInputEventResult ScrollManager::HandleGestureScrollBegin(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollBegin");
  Document* document = frame_->GetDocument();

  if (!document->GetLayoutView())
    return WebInputEventResult::kNotHandled;

  // If there's no layoutObject on the node, send the event to the nearest
  // ancestor with a layoutObject.  Needed for <option> and <optgroup> elements
  // so we can touch scroll <select>s
  while (scroll_gesture_handling_node_ &&
         !scroll_gesture_handling_node_->GetLayoutObject())
    scroll_gesture_handling_node_ =
        scroll_gesture_handling_node_->ParentOrShadowHostNode();

  if (!scroll_gesture_handling_node_)
    scroll_gesture_handling_node_ = frame_->GetDocument();

  if (!scroll_gesture_handling_node_ ||
      !scroll_gesture_handling_node_->GetLayoutObject()) {
    TRACE_EVENT_INSTANT0("input", "Dropping: No LayoutObject",
                         TRACE_EVENT_SCOPE_THREAD);
    return WebInputEventResult::kNotHandled;
  }

  WebInputEventResult child_result = PassScrollGestureEvent(
      gesture_event, scroll_gesture_handling_node_->GetLayoutObject());

  RecordScrollRelatedMetrics(gesture_event.SourceDevice());

  current_scroll_chain_.clear();
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  IntPoint position = FlooredIntPoint(gesture_event.PositionInRootFrame());
  scroll_state_data->position_x = position.X();
  scroll_state_data->position_y = position.Y();
  scroll_state_data->delta_x_hint = -gesture_event.DeltaXInRootFrame();
  scroll_state_data->delta_y_hint = -gesture_event.DeltaYInRootFrame();
  scroll_state_data->is_beginning = true;
  scroll_state_data->from_user_input = true;
  scroll_state_data->is_direct_manipulation =
      gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  ScrollState* scroll_state = ScrollState::Create(std::move(scroll_state_data));
  RecomputeScrollChain(*scroll_gesture_handling_node_.Get(), *scroll_state,
                       current_scroll_chain_);

  TRACE_EVENT_INSTANT1("input", "Computed Scroll Chain",
                       TRACE_EVENT_SCOPE_THREAD, "length",
                       current_scroll_chain_.size());

  if (current_scroll_chain_.empty()) {
    // If a child has a non-empty scroll chain, we need to consider that instead
    // of simply returning WebInputEventResult::kNotHandled.
    return child_result;
  }

  NotifyScrollPhaseBeginForCustomizedScroll(*scroll_state);

  CustomizedScroll(*scroll_state);

  if (gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen)
    UseCounter::Count(frame_->GetDocument(), WebFeature::kScrollByTouch);
  else
    UseCounter::Count(frame_->GetDocument(), WebFeature::kScrollByWheel);

  return WebInputEventResult::kHandledSystem;
}

WebInputEventResult ScrollManager::HandleGestureScrollUpdate(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollUpdate");
  DCHECK_EQ(gesture_event.GetType(), WebInputEvent::kGestureScrollUpdate);

  Node* node = scroll_gesture_handling_node_.Get();
  if (!node || !node->GetLayoutObject()) {
    TRACE_EVENT_INSTANT0("input", "Lost scroll_gesture_handling_node",
                         TRACE_EVENT_SCOPE_THREAD);
    if (previous_gesture_scrolled_node_) {
      // When the scroll_gesture_handling_node_ gets deleted in the middle of
      // scrolling call HandleGestureScrollEvent to start scrolling a new node
      // if possible.
      ClearGestureScrollState();
      WebGestureEvent scroll_begin =
          SynthesizeGestureScrollBegin(gesture_event);
      HandleGestureScrollEvent(scroll_begin);
      node = scroll_gesture_handling_node_.Get();
      if (!node || !node->GetLayoutObject()) {
        TRACE_EVENT_INSTANT0("input", "Failed to find new node",
                             TRACE_EVENT_SCOPE_THREAD);
        return WebInputEventResult::kNotHandled;
      }
    } else {
      TRACE_EVENT_INSTANT0("input", "No previously scrolled node",
                           TRACE_EVENT_SCOPE_THREAD);
      return WebInputEventResult::kNotHandled;
    }
  }
  if (snap_fling_controller_) {
    if (snap_fling_controller_->HandleGestureScrollUpdate(
            GetGestureScrollUpdateInfo(gesture_event))) {
      return WebInputEventResult::kHandledSystem;
    }
  }

  // Negate the deltas since the gesture event stores finger movement and
  // scrolling occurs in the direction opposite the finger's movement
  // direction. e.g. Finger moving up has negative event delta but causes the
  // page to scroll down causing positive scroll delta.
  FloatSize delta(-gesture_event.DeltaXInRootFrame(),
                  -gesture_event.DeltaYInRootFrame());
  FloatSize velocity(-gesture_event.VelocityX(), -gesture_event.VelocityY());
  FloatPoint position(gesture_event.PositionInRootFrame());

  if (delta.IsZero())
    return WebInputEventResult::kNotHandled;

  LayoutObject* layout_object = node->GetLayoutObject();

  // Try to send the event to the correct view.
  WebInputEventResult result =
      PassScrollGestureEvent(gesture_event, layout_object);
  if (result != WebInputEventResult::kNotHandled) {
    // FIXME: we should allow simultaneous scrolling of nested
    // iframes along perpendicular axes. See crbug.com/466991.
    delta_consumed_for_scroll_sequence_ = true;
    return result;
  }

  if (current_scroll_chain_.empty()) {
    TRACE_EVENT_INSTANT0("input", "Empty Scroll Chain",
                         TRACE_EVENT_SCOPE_THREAD);
    return WebInputEventResult::kNotHandled;
  }

  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  scroll_state_data->delta_x = delta.Width();
  scroll_state_data->delta_y = delta.Height();
  scroll_state_data->delta_granularity =
      static_cast<double>(gesture_event.DeltaUnits());
  scroll_state_data->velocity_x = velocity.Width();
  scroll_state_data->velocity_y = velocity.Height();
  scroll_state_data->position_x = position.X();
  scroll_state_data->position_y = position.Y();
  scroll_state_data->is_in_inertial_phase =
      gesture_event.InertialPhase() ==
      WebGestureEvent::InertialPhaseState::kMomentum;
  scroll_state_data->is_direct_manipulation =
      gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen;
  scroll_state_data->from_user_input = true;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  ScrollState* scroll_state = ScrollState::Create(std::move(scroll_state_data));
  if (previous_gesture_scrolled_node_) {
    // The ScrollState needs to know what the current
    // native scrolling element is, so that for an
    // inertial scroll that shouldn't propagate, only the
    // currently scrolling element responds.
    scroll_state->SetCurrentNativeScrollingNode(
        previous_gesture_scrolled_node_);
  }

  CustomizedScroll(*scroll_state);
  previous_gesture_scrolled_node_ = scroll_state->CurrentNativeScrollingNode();
  delta_consumed_for_scroll_sequence_ =
      scroll_state->DeltaConsumedForScrollSequence();

  bool did_scroll_x = scroll_state->deltaX() != delta.Width();
  bool did_scroll_y = scroll_state->deltaY() != delta.Height();

  did_scroll_x_for_scroll_gesture_ |= did_scroll_x;
  did_scroll_y_for_scroll_gesture_ |= did_scroll_y;

  // TODO(bokan): This looks like it resets overscroll if any *effective* root
  // scroller is scrolled. This should probably be if the *global* root
  // scroller is scrolled.
  if (!previous_gesture_scrolled_node_ ||
      !previous_gesture_scrolled_node_->IsEffectiveRootScroller())
    GetPage()->GetOverscrollController().ResetAccumulated(did_scroll_x,
                                                          did_scroll_y);

  if (did_scroll_x || did_scroll_y)
    return WebInputEventResult::kHandledSystem;

  if (RuntimeEnabledFeatures::OverscrollCustomizationEnabled()) {
    // Send the overscroll event to the node that scrolling is latched to which
    // is either previously scrolled node or the last node in the scroll chain.
    Node* overscroll_target = previous_gesture_scrolled_node_;
    if (!overscroll_target && !current_scroll_chain_.empty())
      overscroll_target = DOMNodeIds::NodeForId(current_scroll_chain_.front());

    if (overscroll_target) {
      overscroll_target->GetDocument().EnqueueOverscrollEventForNode(
          overscroll_target, delta.Width(), delta.Height());
    }
  }

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult ScrollManager::HandleGestureScrollEnd(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollEnd");
  Node* node = scroll_gesture_handling_node_;

  if (node && node->GetLayoutObject()) {
    PassScrollGestureEvent(gesture_event, node->GetLayoutObject());
    if (current_scroll_chain_.empty()) {
      ClearGestureScrollState();
      return WebInputEventResult::kNotHandled;
    }
    std::unique_ptr<ScrollStateData> scroll_state_data =
        std::make_unique<ScrollStateData>();
    scroll_state_data->is_ending = true;
    scroll_state_data->is_in_inertial_phase =
        gesture_event.InertialPhase() ==
        WebGestureEvent::InertialPhaseState::kMomentum;
    scroll_state_data->from_user_input = true;
    scroll_state_data->is_direct_manipulation =
        gesture_event.SourceDevice() == WebGestureDevice::kTouchscreen;
    scroll_state_data->delta_consumed_for_scroll_sequence =
        delta_consumed_for_scroll_sequence_;
    ScrollState* scroll_state =
        ScrollState::Create(std::move(scroll_state_data));
    CustomizedScroll(*scroll_state);
    SnapAtGestureScrollEnd();
    NotifyScrollPhaseEndForCustomizedScroll();

    if (RuntimeEnabledFeatures::OverscrollCustomizationEnabled()) {
      // Send the scrollend event to the node that scrolling is latched to
      // which is either previously scrolled node or the last node in the
      // scroll chain.
      DCHECK(!current_scroll_chain_.empty());
      if (previous_gesture_scrolled_node_) {
        previous_gesture_scrolled_node_->GetDocument()
            .EnqueueScrollEndEventForNode(previous_gesture_scrolled_node_);
      } else if (Node* scroll_end_target =
                     DOMNodeIds::NodeForId(current_scroll_chain_.front())) {
        scroll_end_target->GetDocument().EnqueueScrollEndEventForNode(
            scroll_end_target);
      }
    }
  }

  ClearGestureScrollState();

  if (RuntimeEnabledFeatures::UpdateHoverFromScrollAtBeginFrameEnabled())
    frame_->GetEventHandler().MarkHoverStateDirty();

  return WebInputEventResult::kNotHandled;
}

LayoutBox* ScrollManager::LayoutBoxForSnapping() const {
  if (!previous_gesture_scrolled_node_)
    return nullptr;
  return previous_gesture_scrolled_node_->GetLayoutBox();
}

void ScrollManager::SnapAtGestureScrollEnd() {
  if (!previous_gesture_scrolled_node_ ||
      (!did_scroll_x_for_scroll_gesture_ && !did_scroll_y_for_scroll_gesture_))
    return;
  SnapCoordinator* snap_coordinator =
      frame_->GetDocument()->GetSnapCoordinator();
  LayoutBox* layout_box = LayoutBoxForSnapping();
  if (!snap_coordinator || !layout_box)
    return;

  snap_coordinator->SnapAtCurrentPosition(*layout_box,
                                          did_scroll_x_for_scroll_gesture_,
                                          did_scroll_y_for_scroll_gesture_);
}

bool ScrollManager::GetSnapFlingInfo(
    const gfx::Vector2dF& natural_displacement,
    gfx::Vector2dF* out_initial_position,
    gfx::Vector2dF* out_target_position) const {
  SnapCoordinator* snap_coordinator =
      frame_->GetDocument()->GetSnapCoordinator();
  LayoutBox* layout_box = LayoutBoxForSnapping();
  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(layout_box);
  if (!snap_coordinator || !layout_box || !scrollable_area)
    return false;

  FloatPoint current_position = scrollable_area->ScrollPosition();
  *out_initial_position = gfx::Vector2dF(current_position);
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          gfx::ScrollOffset(*out_initial_position),
          gfx::ScrollOffset(natural_displacement));
  base::Optional<FloatPoint> snap_end =
      snap_coordinator->GetSnapPosition(*layout_box, *strategy);
  if (!snap_end.has_value())
    return false;
  *out_target_position = gfx::Vector2dF(snap_end.value());
  return true;
}

gfx::Vector2dF ScrollManager::ScrollByForSnapFling(
    const gfx::Vector2dF& delta) {
  ScrollableArea* scrollable_area =
      ScrollableAreaForSnapping(LayoutBoxForSnapping());
  if (!scrollable_area)
    return gfx::Vector2dF();

  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();

  // TODO(sunyunjia): Plumb the velocity of the snap curve as well.
  scroll_state_data->delta_x = delta.x();
  scroll_state_data->delta_y = delta.y();
  scroll_state_data->is_in_inertial_phase = true;
  scroll_state_data->from_user_input = true;
  scroll_state_data->delta_granularity =
      static_cast<double>(ScrollGranularity::kScrollByPrecisePixel);
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  ScrollState* scroll_state = ScrollState::Create(std::move(scroll_state_data));
  scroll_state->SetCurrentNativeScrollingNode(previous_gesture_scrolled_node_);

  CustomizedScroll(*scroll_state);
  FloatPoint end_position = scrollable_area->ScrollPosition();
  return gfx::Vector2dF(end_position.X(), end_position.Y());
}

void ScrollManager::ScrollEndForSnapFling() {
  if (current_scroll_chain_.empty()) {
    NotifyScrollPhaseEndForCustomizedScroll();
    ClearGestureScrollState();
    return;
  }
  std::unique_ptr<ScrollStateData> scroll_state_data =
      std::make_unique<ScrollStateData>();
  scroll_state_data->is_ending = true;
  scroll_state_data->is_in_inertial_phase = true;
  scroll_state_data->from_user_input = true;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  ScrollState* scroll_state = ScrollState::Create(std::move(scroll_state_data));
  CustomizedScroll(*scroll_state);
  NotifyScrollPhaseEndForCustomizedScroll();
  ClearGestureScrollState();
}

void ScrollManager::RequestAnimationForSnapFling() {
  if (Page* page = frame_->GetPage())
    page->GetChromeClient().ScheduleAnimation(frame_->View());
}

void ScrollManager::AnimateSnapFling(base::TimeTicks monotonic_time) {
  DCHECK(snap_fling_controller_);
  snap_fling_controller_->Animate(monotonic_time);
}

Page* ScrollManager::GetPage() const {
  return frame_->GetPage();
}

WebInputEventResult ScrollManager::PassScrollGestureEvent(
    const WebGestureEvent& gesture_event,
    LayoutObject* layout_object) {
  DCHECK(gesture_event.IsScrollEvent());

  if (!last_gesture_scroll_over_embedded_content_view_ || !layout_object ||
      !layout_object->IsLayoutEmbeddedContent())
    return WebInputEventResult::kNotHandled;

  FrameView* frame_view =
      ToLayoutEmbeddedContent(layout_object)->ChildFrameView();

  if (!frame_view)
    return WebInputEventResult::kNotHandled;

  auto* local_frame_view = DynamicTo<LocalFrameView>(frame_view);
  if (!local_frame_view)
    return WebInputEventResult::kNotHandled;

  return local_frame_view->GetFrame()
      .GetEventHandler()
      .HandleGestureScrollEvent(gesture_event);
}

WebInputEventResult ScrollManager::HandleGestureScrollEvent(
    const WebGestureEvent& gesture_event) {
  if (!frame_->View())
    return WebInputEventResult::kNotHandled;

  TRACE_EVENT0("input", "ScrollManager::handleGestureScrollEvent");

  Node* event_target = nullptr;
  Scrollbar* scrollbar = nullptr;
  if (gesture_event.GetType() != WebInputEvent::kGestureScrollBegin) {
    scrollbar = scrollbar_handling_scroll_gesture_.Get();
    event_target = scroll_gesture_handling_node_.Get();
  } else if (gesture_event.GetType() == WebInputEvent::kGestureScrollBegin &&
             gesture_event.data.scroll_begin.scrollable_area_element_id) {
    CompositorElementId element_id = CompositorElementId(
        gesture_event.data.scroll_begin.scrollable_area_element_id);
    event_target = NodeTargetForScrollableAreaElementId(element_id);
    if (!event_target) {
      // If we couldn't find a node associated with the targeted scrollable
      // area, just drop the gesture. This may be due to the fact that the
      // targeted has been removed from the tree between when the gesture
      // was queued and when we handle it.
      return WebInputEventResult::kNotHandled;
    }

    ClearGestureScrollState();
    scroll_gesture_handling_node_ = event_target;
    if (event_target->GetLayoutObject()->IsLayoutEmbeddedContent()) {
      last_gesture_scroll_over_embedded_content_view_ = true;
    }
  }

  if (!event_target) {
    Document* document = frame_->GetDocument();
    if (!document->GetLayoutView())
      return WebInputEventResult::kNotHandled;

    TRACE_EVENT_INSTANT0("input", "Retargeting Scroll",
                         TRACE_EVENT_SCOPE_THREAD);

    LocalFrameView* view = frame_->View();
    LayoutPoint view_point = view->ConvertFromRootFrame(
        FlooredIntPoint(gesture_event.PositionInRootFrame()));
    HitTestRequest request(HitTestRequest::kReadOnly);
    HitTestLocation location(view_point);
    HitTestResult result(request, location);
    document->GetLayoutView()->HitTest(location, result);

    event_target = result.InnerNode();

    last_gesture_scroll_over_embedded_content_view_ =
        result.IsOverEmbeddedContentView();

    scroll_gesture_handling_node_ = event_target;
    previous_gesture_scrolled_node_ = nullptr;
    delta_consumed_for_scroll_sequence_ = false;
    did_scroll_x_for_scroll_gesture_ = false;
    did_scroll_y_for_scroll_gesture_ = false;

    if (!scrollbar)
      scrollbar = result.GetScrollbar();
  }

  // Gesture scroll events injected by scrollbars should not be routed back to
  // the scrollbar itself as they are intended to perform the scroll action on
  // the scrollable area. Scrollbar injected gestures don't clear
  // scrollbar_handling_scroll_gesture_ because touch-based scroll gestures need
  // to continue going to the scrollbar first so that the scroll direction
  // can be made proportional to the scroll thumb/ScrollableArea size and
  // inverted.
  if (scrollbar &&
      gesture_event.SourceDevice() != WebGestureDevice::kScrollbar) {
    bool should_update_capture = false;
    if (scrollbar->GestureEvent(gesture_event, &should_update_capture)) {
      if (should_update_capture)
        scrollbar_handling_scroll_gesture_ = scrollbar;
      return WebInputEventResult::kHandledSuppressed;
    }

    scrollbar_handling_scroll_gesture_ = nullptr;
  }

  if (event_target) {
    if (HandleScrollGestureOnResizer(event_target, gesture_event))
      return WebInputEventResult::kHandledSuppressed;

    GestureEvent* gesture_dom_event = GestureEvent::Create(
        event_target->GetDocument().domWindow(), gesture_event);
    if (gesture_dom_event) {
      DispatchEventResult gesture_dom_event_result =
          event_target->DispatchEvent(*gesture_dom_event);
      if (gesture_dom_event_result != DispatchEventResult::kNotCanceled) {
        DCHECK(gesture_dom_event_result !=
               DispatchEventResult::kCanceledByEventHandler);
        return event_handling_util::ToWebInputEventResult(
            gesture_dom_event_result);
      }
    }
  }

  if (snap_fling_controller_) {
    if (gesture_event.IsGestureScroll() &&
        (snap_fling_controller_->FilterEventForSnap(
            ToGestureScrollType(gesture_event.GetType())))) {
      return WebInputEventResult::kNotHandled;
    }
  }
  switch (gesture_event.GetType()) {
    case WebInputEvent::kGestureScrollBegin:
      return HandleGestureScrollBegin(gesture_event);
    case WebInputEvent::kGestureScrollUpdate:
      return HandleGestureScrollUpdate(gesture_event);
    case WebInputEvent::kGestureScrollEnd:
      return HandleGestureScrollEnd(gesture_event);
    case WebInputEvent::kGestureFlingStart:
    case WebInputEvent::kGestureFlingCancel:
      return WebInputEventResult::kNotHandled;
    default:
      NOTREACHED();
      return WebInputEventResult::kNotHandled;
  }
}

Node* ScrollManager::NodeTargetForScrollableAreaElementId(
    CompositorElementId element_id) const {
  Page* page = frame_->GetPage();
  DCHECK(page);
  ScrollableArea* scrollable_area = nullptr;
  if (page->GetVisualViewport().GetCompositorElementId() == element_id) {
    // If the element_id is the visual viewport, redirect to the
    // root LocalFrameView's scrollable area (i.e. the RootFrameViewport).
    scrollable_area = frame_->LocalFrameRoot().View()->GetScrollableArea();
  } else {
    ScrollingCoordinator* scrolling_coordinator =
        page->GetScrollingCoordinator();
    scrollable_area =
        scrolling_coordinator->ScrollableAreaWithElementIdInAllLocalFrames(
            element_id);
  }

  // It is possible for an unrelated task to run between the time that
  // the gesture targeting element_id is queued and when we're processing
  // the gesture here, so we must validate the scrollable_area still
  // exists along with it's layout information. If not, just drop this
  // gesture since there is no relevant data on where to target the gesture.
  LayoutBox* layout_box =
      scrollable_area ? scrollable_area->GetLayoutBox() : nullptr;
  if (!layout_box) {
    return nullptr;
  }

  Node* event_target = nullptr;
  if (layout_box->GetDocument().GetFrame() == frame_) {
    event_target = scrollable_area->GetLayoutBox()->GetNode();
  } else {
    // The targeted ScrollableArea may not belong to this frame. If that
    // is the case, target its ancestor HTMLFrameOwnerElement that exists
    // in this view, so that the gesture handling can be passed down to
    // the appropriate event handler.
    LocalFrame* current_frame = layout_box->GetDocument().GetFrame();
    while (current_frame) {
      HTMLFrameOwnerElement* owner = current_frame->GetDocument()->LocalOwner();
      LocalFrame* owner_frame =
          owner ? owner->GetDocument().GetFrame() : nullptr;
      if (owner_frame == frame_) {
        event_target = owner;
        break;
      }
      current_frame = owner_frame;
    }
  }

  return event_target;
}

bool ScrollManager::IsScrollbarHandlingGestures() const {
  return scrollbar_handling_scroll_gesture_.Get();
}

bool ScrollManager::HandleScrollGestureOnResizer(
    Node* event_target,
    const WebGestureEvent& gesture_event) {
  if (gesture_event.SourceDevice() != WebGestureDevice::kTouchscreen)
    return false;

  if (gesture_event.GetType() == WebInputEvent::kGestureScrollBegin) {
    PaintLayer* layer = event_target->GetLayoutObject()
                            ? event_target->GetLayoutObject()->EnclosingLayer()
                            : nullptr;
    IntPoint p = frame_->View()->ConvertFromRootFrame(
        FlooredIntPoint(gesture_event.PositionInRootFrame()));
    if (layer && layer->GetScrollableArea() &&
        layer->GetScrollableArea()->IsPointInResizeControl(p,
                                                           kResizerForTouch)) {
      resize_scrollable_area_ = layer->GetScrollableArea();
      resize_scrollable_area_->SetInResizeMode(true);
      offset_from_resize_corner_ =
          LayoutSize(resize_scrollable_area_->OffsetFromResizeCorner(p));
      return true;
    }
  } else if (gesture_event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    if (resize_scrollable_area_ && resize_scrollable_area_->InResizeMode()) {
      IntPoint pos = RoundedIntPoint(gesture_event.PositionInRootFrame());
      pos.Move(gesture_event.DeltaXInRootFrame(),
               gesture_event.DeltaYInRootFrame());
      resize_scrollable_area_->Resize(pos, offset_from_resize_corner_);
      return true;
    }
  } else if (gesture_event.GetType() == WebInputEvent::kGestureScrollEnd) {
    if (resize_scrollable_area_ && resize_scrollable_area_->InResizeMode()) {
      resize_scrollable_area_->SetInResizeMode(false);
      resize_scrollable_area_ = nullptr;
      return false;
    }
  }

  return false;
}

bool ScrollManager::InResizeMode() const {
  return resize_scrollable_area_ && resize_scrollable_area_->InResizeMode();
}

void ScrollManager::Resize(const WebMouseEvent& evt) {
  if (evt.GetType() == WebInputEvent::kMouseMove) {
    if (!frame_->GetEventHandler().MousePressed())
      return;
    resize_scrollable_area_->Resize(FlooredIntPoint(evt.PositionInRootFrame()),
                                    offset_from_resize_corner_);
  }
}

void ScrollManager::ClearResizeScrollableArea(bool should_not_be_null) {
  if (should_not_be_null)
    DCHECK(resize_scrollable_area_);

  if (resize_scrollable_area_)
    resize_scrollable_area_->SetInResizeMode(false);
  resize_scrollable_area_ = nullptr;
}

void ScrollManager::SetResizeScrollableArea(PaintLayer* layer, IntPoint p) {
  resize_scrollable_area_ = layer->GetScrollableArea();
  resize_scrollable_area_->SetInResizeMode(true);
  offset_from_resize_corner_ =
      LayoutSize(resize_scrollable_area_->OffsetFromResizeCorner(p));
}

bool ScrollManager::CanHandleGestureEvent(
    const GestureEventWithHitTestResults& targeted_event) {
  Scrollbar* scrollbar = targeted_event.GetHitTestResult().GetScrollbar();

  if (scrollbar) {
    bool should_update_capture = false;
    if (scrollbar->GestureEvent(targeted_event.Event(),
                                &should_update_capture)) {
      if (should_update_capture)
        scrollbar_handling_scroll_gesture_ = scrollbar;
      return true;
    }
  }
  return false;
}

WebGestureEvent ScrollManager::SynthesizeGestureScrollBegin(
    const WebGestureEvent& update_event) {
  DCHECK_EQ(update_event.GetType(), WebInputEvent::kGestureScrollUpdate);
  WebGestureEvent scroll_begin(update_event);
  scroll_begin.SetType(WebInputEvent::kGestureScrollBegin);
  scroll_begin.data.scroll_begin.delta_x_hint =
      update_event.data.scroll_update.delta_x;
  scroll_begin.data.scroll_begin.delta_y_hint =
      update_event.data.scroll_update.delta_y;
  scroll_begin.data.scroll_begin.delta_hint_units =
      update_event.data.scroll_update.delta_units;
  scroll_begin.data.scroll_begin.scrollable_area_element_id = 0;
  return scroll_begin;
}

void ScrollManager::NotifyScrollPhaseBeginForCustomizedScroll(
    const ScrollState& scroll_state) {
  scroll_customization::ScrollDirection direction =
      scroll_customization::GetScrollDirectionFromDeltas(
          scroll_state.deltaXHint(), scroll_state.deltaYHint());
  for (auto id : current_scroll_chain_) {
    Node* node = DOMNodeIds::NodeForId(id);
    if (node)
      node->WillBeginCustomizedScrollPhase(direction);
  }
}

void ScrollManager::NotifyScrollPhaseEndForCustomizedScroll() {
  for (auto id : current_scroll_chain_) {
    Node* node = DOMNodeIds::NodeForId(id);
    if (node)
      node->DidEndCustomizedScrollPhase();
  }
}

}  // namespace blink
