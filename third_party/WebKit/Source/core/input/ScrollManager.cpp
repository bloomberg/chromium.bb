// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/ScrollManager.h"

#include <memory>
#include "core/dom/DOMNodeIds.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/events/GestureEvent.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/input/EventHandlingUtil.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "core/page/scrolling/ScrollState.h"
#include "core/paint/PaintLayer.h"
#include "platform/Histogram.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

ScrollManager::ScrollManager(LocalFrame& frame) : frame_(frame) {
  Clear();
}

void ScrollManager::Clear() {
  last_gesture_scroll_over_embedded_content_view_ = false;
  scrollbar_handling_scroll_gesture_ = nullptr;
  resize_scrollable_area_ = nullptr;
  offset_from_resize_corner_ = LayoutSize();
  ClearGestureScrollState();
}

void ScrollManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(scroll_gesture_handling_node_);
  visitor->Trace(previous_gesture_scrolled_element_);
  visitor->Trace(scrollbar_handling_scroll_gesture_);
  visitor->Trace(resize_scrollable_area_);
}

void ScrollManager::ClearGestureScrollState() {
  scroll_gesture_handling_node_ = nullptr;
  previous_gesture_scrolled_element_ = nullptr;
  delta_consumed_for_scroll_sequence_ = false;
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

static bool CanPropagate(const ScrollState& scroll_state,
                         const Element& element) {
  if (!element.GetLayoutBox()->GetScrollableArea())
    return true;

  return (scroll_state.deltaXHint() == 0 ||
          element.GetComputedStyle()->ScrollBoundaryBehaviorX() ==
              EScrollBoundaryBehavior::kAuto) &&
         (scroll_state.deltaYHint() == 0 ||
          element.GetComputedStyle()->ScrollBoundaryBehaviorY() ==
              EScrollBoundaryBehavior::kAuto);
}

void ScrollManager::RecomputeScrollChain(const Node& start_node,
                                         const ScrollState& scroll_state,
                                         std::deque<int>& scroll_chain) {
  DCHECK(!scroll_chain.size());
  scroll_chain.clear();

  DCHECK(start_node.GetLayoutObject());
  LayoutBox* cur_box = start_node.GetLayoutObject()->EnclosingBox();
  Element* document_element = frame_->GetDocument()->documentElement();

  // Scrolling propagates along the containing block chain and ends at the
  // RootScroller element. The RootScroller element will have a custom
  // applyScroll callback that scrolls the frame or element.
  while (cur_box) {
    Node* cur_node = cur_box->GetNode();
    Element* cur_element = nullptr;

    // FIXME: this should reject more elements, as part of crbug.com/410974.
    if (cur_node && cur_node->IsElementNode()) {
      cur_element = ToElement(cur_node);
    } else if (cur_node && cur_node->IsDocumentNode()) {
      // In normal circumastances, the documentElement will be the root
      // scroller but the documentElement itself isn't a containing block,
      // that'll be the document node rather than the element.
      cur_element = document_element;
    }

    if (cur_element) {
      if (CanScroll(scroll_state, *cur_element))
        scroll_chain.push_front(DOMNodeIds::IdForNode(cur_element));
      if (IsViewportScrollingElement(*cur_element) ||
          cur_element == document_element)
        break;

      if (!CanPropagate(scroll_state, *cur_element))
        break;
    }

    cur_box = cur_box->ContainingBlock();
  }
}

bool ScrollManager::CanScroll(const ScrollState& scroll_state,
                              const Element& current_element) {
  const double delta_x = scroll_state.isBeginning() ? scroll_state.deltaXHint()
                                                    : scroll_state.deltaX();
  const double delta_y = scroll_state.isBeginning() ? scroll_state.deltaYHint()
                                                    : scroll_state.deltaY();
  if (!delta_x && !delta_y)
    return true;

  ScrollableArea* scrollable_area = nullptr;

  if (IsViewportScrollingElement(current_element) ||
      current_element == *(frame_->GetDocument()->documentElement())) {
    if (!frame_->Tree().Parent() || frame_->Tree().Parent()->IsLocalFrame())
      return true;

    // For oopif the viewport is added to scroll chain only if it can actually
    // consume some delta hints.
    DCHECK(frame_->Tree().Parent()->IsRemoteFrame());
    scrollable_area =
        frame_->View() ? frame_->View()->GetScrollableArea() : nullptr;
  }

  if (!scrollable_area && current_element.GetLayoutBox())
    scrollable_area = current_element.GetLayoutBox()->GetScrollableArea();

  if (!scrollable_area)
    return false;

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

  if ((!node || !node->GetLayoutObject()) && frame_->View() &&
      !frame_->View()->GetLayoutViewItem().IsNull())
    node = frame_->View()->GetLayoutViewItem().GetNode();

  if (!node)
    return false;

  frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  LayoutBox* cur_box = node->GetLayoutObject()->EnclosingBox();
  while (cur_box) {
    ScrollDirectionPhysical physical_direction =
        ToPhysicalDirection(direction, cur_box->IsHorizontalWritingMode(),
                            cur_box->Style()->IsFlippedBlocksWritingMode());

    ScrollResult result =
        cur_box->Scroll(granularity, ToScrollDelta(physical_direction, 1));

    if (result.DidScroll())
      return true;

    cur_box = cur_box->ContainingBlock();
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
  frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  // FIXME: enable scroll customization in this case. See crbug.com/410974.
  if (LogicalScroll(direction, granularity, starting_node, mouse_press_node))
    return true;

  Frame* parent_frame = frame_->Tree().Parent();
  if (!parent_frame || !parent_frame->IsLocalFrame())
    return false;
  // FIXME: Broken for OOPI.
  return ToLocalFrame(parent_frame)
      ->GetEventHandler()
      .BubblingScroll(direction, granularity, frame_->DeprecatedLocalOwner());
}

void ScrollManager::CustomizedScroll(ScrollState& scroll_state) {
  if (scroll_state.FullyConsumed())
    return;

  if (scroll_state.deltaX() || scroll_state.deltaY())
    frame_->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

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

    DCHECK(!scrollable_area->UsesCompositedScrolling() ||
           !scrollable_area->GetNonCompositedMainThreadScrollingReasons());
    *non_composited_main_thread_scrolling_reasons |=
        scrollable_area->GetNonCompositedMainThreadScrollingReasons();
  }
}

void ScrollManager::RecordScrollRelatedMetrics(const WebGestureDevice device) {
  if (device != kWebGestureDeviceTouchpad &&
      device != kWebGestureDeviceTouchscreen) {
    return;
  }

  uint32_t non_composited_main_thread_scrolling_reasons = 0;
  ComputeScrollRelatedMetrics(&non_composited_main_thread_scrolling_reasons);

  if (non_composited_main_thread_scrolling_reasons) {
    DCHECK(MainThreadScrollingReason::HasNonCompositedScrollReasons(
        non_composited_main_thread_scrolling_reasons));
    uint32_t main_thread_scrolling_reason_enum_max =
        MainThreadScrollingReason::kMainThreadScrollingReasonCount + 1;
    for (uint32_t i = MainThreadScrollingReason::kNonCompositedReasonsFirst;
         i <= MainThreadScrollingReason::kNonCompositedReasonsLast; ++i) {
      unsigned val = 1 << i;
      if (non_composited_main_thread_scrolling_reasons & val) {
        if (device == kWebGestureDeviceTouchscreen) {
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
  Document* document = frame_->GetDocument();

  if (document->GetLayoutViewItem().IsNull())
    return WebInputEventResult::kNotHandled;

  // If there's no layoutObject on the node, send the event to the nearest
  // ancestor with a layoutObject.  Needed for <option> and <optgroup> elements
  // so we can touch scroll <select>s
  while (scroll_gesture_handling_node_ &&
         !scroll_gesture_handling_node_->GetLayoutObject())
    scroll_gesture_handling_node_ =
        scroll_gesture_handling_node_->ParentOrShadowHostNode();

  if (!scroll_gesture_handling_node_)
    scroll_gesture_handling_node_ = frame_->GetDocument()->documentElement();

  if (!scroll_gesture_handling_node_ ||
      !scroll_gesture_handling_node_->GetLayoutObject())
    return WebInputEventResult::kNotHandled;

  PassScrollGestureEvent(gesture_event,
                         scroll_gesture_handling_node_->GetLayoutObject());

  RecordScrollRelatedMetrics(gesture_event.source_device);

  current_scroll_chain_.clear();
  std::unique_ptr<ScrollStateData> scroll_state_data =
      WTF::MakeUnique<ScrollStateData>();
  IntPoint position = FlooredIntPoint(gesture_event.PositionInRootFrame());
  scroll_state_data->position_x = position.X();
  scroll_state_data->position_y = position.Y();
  scroll_state_data->delta_x_hint = -gesture_event.DeltaXInRootFrame();
  scroll_state_data->delta_y_hint = -gesture_event.DeltaYInRootFrame();
  scroll_state_data->is_beginning = true;
  scroll_state_data->from_user_input = true;
  scroll_state_data->is_direct_manipulation =
      gesture_event.source_device == kWebGestureDeviceTouchscreen;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  ScrollState* scroll_state = ScrollState::Create(std::move(scroll_state_data));
  RecomputeScrollChain(*scroll_gesture_handling_node_.Get(), *scroll_state,
                       current_scroll_chain_);
  if (current_scroll_chain_.empty())
    return WebInputEventResult::kNotHandled;

  CustomizedScroll(*scroll_state);

  if (gesture_event.source_device == kWebGestureDeviceTouchscreen)
    UseCounter::Count(frame_->GetDocument(), WebFeature::kScrollByTouch);
  else
    UseCounter::Count(frame_->GetDocument(), WebFeature::kScrollByWheel);

  return WebInputEventResult::kHandledSystem;
}

WebInputEventResult ScrollManager::HandleGestureScrollUpdate(
    const WebGestureEvent& gesture_event) {
  DCHECK_EQ(gesture_event.GetType(), WebInputEvent::kGestureScrollUpdate);

  Node* node = scroll_gesture_handling_node_.Get();
  if (!node || !node->GetLayoutObject()) {
    if (previous_gesture_scrolled_element_) {
      // When the scroll_gesture_handling_node_ gets deleted in the middle of
      // scrolling call HandleGestureScrollEvent to start scrolling a new node
      // if possible.
      ClearGestureScrollState();
      WebGestureEvent scroll_begin =
          SynthesizeGestureScrollBegin(gesture_event);
      HandleGestureScrollEvent(scroll_begin);
      node = scroll_gesture_handling_node_.Get();
      if (!node || !node->GetLayoutObject())
        return WebInputEventResult::kNotHandled;
    } else {
      return WebInputEventResult::kNotHandled;
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

  if (current_scroll_chain_.empty())
    return WebInputEventResult::kNotHandled;

  std::unique_ptr<ScrollStateData> scroll_state_data =
      WTF::MakeUnique<ScrollStateData>();
  scroll_state_data->delta_x = delta.Width();
  scroll_state_data->delta_y = delta.Height();
  scroll_state_data->delta_granularity = static_cast<double>(
      ToPlatformScrollGranularity(gesture_event.DeltaUnits()));
  scroll_state_data->velocity_x = velocity.Width();
  scroll_state_data->velocity_y = velocity.Height();
  scroll_state_data->position_x = position.X();
  scroll_state_data->position_y = position.Y();
  scroll_state_data->is_in_inertial_phase =
      gesture_event.InertialPhase() == WebGestureEvent::kMomentumPhase;
  scroll_state_data->is_direct_manipulation =
      gesture_event.source_device == kWebGestureDeviceTouchscreen;
  scroll_state_data->from_user_input = true;
  scroll_state_data->delta_consumed_for_scroll_sequence =
      delta_consumed_for_scroll_sequence_;
  ScrollState* scroll_state = ScrollState::Create(std::move(scroll_state_data));
  if (previous_gesture_scrolled_element_) {
    // The ScrollState needs to know what the current
    // native scrolling element is, so that for an
    // inertial scroll that shouldn't propagate, only the
    // currently scrolling element responds.
    scroll_state->SetCurrentNativeScrollingElement(
        previous_gesture_scrolled_element_);
  }

  CustomizedScroll(*scroll_state);
  previous_gesture_scrolled_element_ =
      scroll_state->CurrentNativeScrollingElement();
  delta_consumed_for_scroll_sequence_ =
      scroll_state->DeltaConsumedForScrollSequence();

  bool did_scroll_x = scroll_state->deltaX() != delta.Width();
  bool did_scroll_y = scroll_state->deltaY() != delta.Height();

  if ((!previous_gesture_scrolled_element_ ||
       !IsViewportScrollingElement(*previous_gesture_scrolled_element_)) &&
      GetPage())
    GetPage()->GetOverscrollController().ResetAccumulated(did_scroll_x,
                                                          did_scroll_y);

  if (did_scroll_x || did_scroll_y)
    return WebInputEventResult::kHandledSystem;

  return WebInputEventResult::kNotHandled;
}

WebInputEventResult ScrollManager::HandleGestureScrollEnd(
    const WebGestureEvent& gesture_event) {
  Node* node = scroll_gesture_handling_node_;

  if (node && node->GetLayoutObject()) {
    PassScrollGestureEvent(gesture_event, node->GetLayoutObject());
    if (current_scroll_chain_.empty()) {
      ClearGestureScrollState();
      return WebInputEventResult::kNotHandled;
    }
    std::unique_ptr<ScrollStateData> scroll_state_data =
        WTF::MakeUnique<ScrollStateData>();
    scroll_state_data->is_ending = true;
    scroll_state_data->is_in_inertial_phase =
        gesture_event.InertialPhase() == WebGestureEvent::kMomentumPhase;
    scroll_state_data->from_user_input = true;
    scroll_state_data->is_direct_manipulation =
        gesture_event.source_device == kWebGestureDeviceTouchscreen;
    scroll_state_data->delta_consumed_for_scroll_sequence =
        delta_consumed_for_scroll_sequence_;
    ScrollState* scroll_state =
        ScrollState::Create(std::move(scroll_state_data));
    CustomizedScroll(*scroll_state);
  }

  ClearGestureScrollState();
  return WebInputEventResult::kNotHandled;
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

  LocalFrameView* frame_view =
      ToLayoutEmbeddedContent(layout_object)->ChildFrameView();

  if (!frame_view)
    return WebInputEventResult::kNotHandled;

  return frame_view->GetFrame().GetEventHandler().HandleGestureScrollEvent(
      gesture_event);
}

bool ScrollManager::IsViewportScrollingElement(const Element& element) const {
  // The root scroller is the one Element on the page designated to perform
  // "viewport actions" like browser controls movement and overscroll glow.
  if (!frame_->GetDocument())
    return false;

  return frame_->GetDocument()->GetRootScrollerController().ScrollsViewport(
      element);
}

WebInputEventResult ScrollManager::HandleGestureScrollEvent(
    const WebGestureEvent& gesture_event) {
  if (!frame_->View())
    return WebInputEventResult::kNotHandled;

  Node* event_target = nullptr;
  Scrollbar* scrollbar = nullptr;
  if (gesture_event.GetType() != WebInputEvent::kGestureScrollBegin) {
    scrollbar = scrollbar_handling_scroll_gesture_.Get();
    event_target = scroll_gesture_handling_node_.Get();
  }

  if (!event_target) {
    Document* document = frame_->GetDocument();
    if (document->GetLayoutViewItem().IsNull())
      return WebInputEventResult::kNotHandled;

    LocalFrameView* view = frame_->View();
    LayoutPoint view_point = view->RootFrameToContents(
        FlooredIntPoint(gesture_event.PositionInRootFrame()));
    HitTestRequest request(HitTestRequest::kReadOnly);
    HitTestResult result(request, view_point);
    document->GetLayoutViewItem().HitTest(result);

    event_target = result.InnerNode();

    last_gesture_scroll_over_embedded_content_view_ =
        result.IsOverEmbeddedContentView();
    scroll_gesture_handling_node_ = event_target;
    previous_gesture_scrolled_element_ = nullptr;
    delta_consumed_for_scroll_sequence_ = false;

    if (!scrollbar)
      scrollbar = result.GetScrollbar();
  }

  if (scrollbar) {
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
          event_target->DispatchEvent(gesture_dom_event);
      if (gesture_dom_event_result != DispatchEventResult::kNotCanceled) {
        DCHECK(gesture_dom_event_result !=
               DispatchEventResult::kCanceledByEventHandler);
        return EventHandlingUtil::ToWebInputEventResult(
            gesture_dom_event_result);
      }
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
    case WebInputEvent::kGesturePinchBegin:
    case WebInputEvent::kGesturePinchEnd:
    case WebInputEvent::kGesturePinchUpdate:
      return WebInputEventResult::kNotHandled;
    default:
      NOTREACHED();
      return WebInputEventResult::kNotHandled;
  }
}

bool ScrollManager::IsScrollbarHandlingGestures() const {
  return scrollbar_handling_scroll_gesture_.Get();
}

bool ScrollManager::HandleScrollGestureOnResizer(
    Node* event_target,
    const WebGestureEvent& gesture_event) {
  if (gesture_event.source_device != kWebGestureDeviceTouchscreen)
    return false;

  if (gesture_event.GetType() == WebInputEvent::kGestureScrollBegin) {
    PaintLayer* layer = event_target->GetLayoutObject()
                            ? event_target->GetLayoutObject()->EnclosingLayer()
                            : nullptr;
    IntPoint p = frame_->View()->RootFrameToContents(
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
  return scroll_begin;
}

}  // namespace blink
