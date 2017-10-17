/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EventHandler_h
#define EventHandler_h

#include "core/CoreExport.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/TextEventInputType.h"
#include "core/input/GestureManager.h"
#include "core/input/KeyboardEventManager.h"
#include "core/input/MouseEventManager.h"
#include "core/input/MouseWheelEventManager.h"
#include "core/input/PointerEventManager.h"
#include "core/input/ScrollManager.h"
#include "core/layout/HitTestRequest.h"
#include "core/page/DragActions.h"
#include "core/page/EventWithHitTestResults.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/Cursor.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashTraits.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebMenuSourceType.h"

namespace blink {

class DataTransfer;
class PaintLayer;
class Element;
class Event;
class EventTarget;
template <typename EventType>
class EventWithHitTestResults;
class FloatQuad;
class HTMLFrameSetElement;
class HitTestRequest;
class HitTestResult;
class LayoutObject;
class LocalFrame;
class Node;
class ScrollableArea;
class Scrollbar;
class SelectionController;
class TextEvent;
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;

class CORE_EXPORT EventHandler final
    : public GarbageCollectedFinalized<EventHandler> {
  WTF_MAKE_NONCOPYABLE(EventHandler);

 public:
  explicit EventHandler(LocalFrame&);
  DECLARE_TRACE();

  void Clear();

  void UpdateSelectionForMouseDrag();
  void StartMiddleClickAutoscroll(LayoutObject*);

  // TODO(nzolghadr): Some of the APIs in this class only forward the action
  // to the corresponding Manager class. We need to investigate whether it is
  // better to expose the manager instance itself later or can the access to
  // those APIs be more limited or removed.

  void StopAutoscroll();

  void DispatchFakeMouseMoveEventSoon(MouseEventManager::FakeMouseMoveReason);
  void DispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

  HitTestResult HitTestResultAtPoint(
      const LayoutPoint&,
      HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kReadOnly |
                                                    HitTestRequest::kActive,
      const LayoutSize& padding = LayoutSize());

  bool MousePressed() const { return mouse_event_manager_->MousePressed(); }
  bool IsMousePositionUnknown() const {
    return mouse_event_manager_->IsMousePositionUnknown();
  }
  void ClearMouseEventManager() const { mouse_event_manager_->Clear(); }
  void SetCapturingMouseEventsNode(
      Node*);  // A caller is responsible for resetting capturing node to 0.

  WebInputEventResult UpdateDragAndDrop(const WebMouseEvent&, DataTransfer*);
  void CancelDragAndDrop(const WebMouseEvent&, DataTransfer*);
  WebInputEventResult PerformDragAndDrop(const WebMouseEvent&, DataTransfer*);
  void UpdateDragStateAfterEditDragIfNeeded(Element* root_editable_element);

  void ScheduleHoverStateUpdate();
  void ScheduleCursorUpdate();

  // Return whether a mouse cursor update is currently pending.  Used for
  // testing.
  bool CursorUpdatePending();

  // Return whether sending a fake mouse move is currently pending.  Used for
  // testing.
  bool FakeMouseMovePending() const;

  void SetResizingFrameSet(HTMLFrameSetElement*);

  void ResizeScrollableAreaDestroyed();

  IntPoint LastKnownMousePosition() const;

  IntPoint DragDataTransferLocationForTesting();

  // Performs a logical scroll that chains, crossing frames, starting from
  // the given node or a reasonable default (focus/last clicked).
  bool BubblingScroll(ScrollDirection,
                      ScrollGranularity,
                      Node* starting_node = nullptr);

  WebInputEventResult HandleMouseMoveEvent(
      const WebMouseEvent&,
      const Vector<WebMouseEvent>& coalesced_events);
  void HandleMouseLeaveEvent(const WebMouseEvent&);

  WebInputEventResult HandlePointerEvent(const WebPointerEvent&, Node* target);

  WebInputEventResult HandleMousePressEvent(const WebMouseEvent&);
  WebInputEventResult HandleMouseReleaseEvent(const WebMouseEvent&);
  WebInputEventResult HandleWheelEvent(const WebMouseWheelEvent&);

  // Called on the local root frame exactly once per gesture event.
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&);
  WebInputEventResult HandleGestureEvent(const GestureEventWithHitTestResults&);

  // Clear the old hover/active state within frames before moving the hover
  // state to the another frame
  void UpdateGestureHoverActiveState(const HitTestRequest&, Element*);

  // Hit-test the provided (non-scroll) gesture event, applying touch-adjustment
  // and updating hover/active state across all frames if necessary. This should
  // be called at most once per gesture event, and called on the local root
  // frame.
  // Note: This is similar to (the less clearly named) prepareMouseEvent.
  // FIXME: Remove readOnly param when there is only ever a single call to this.
  GestureEventWithHitTestResults TargetGestureEvent(const WebGestureEvent&,
                                                    bool read_only = false);
  GestureEventWithHitTestResults HitTestResultForGestureEvent(
      const WebGestureEvent&,
      HitTestRequest::HitTestRequestType);
  // Handle the provided non-scroll gesture event. Should be called only on the
  // inner frame.
  WebInputEventResult HandleGestureEventInFrame(
      const GestureEventWithHitTestResults&);

  // Handle the provided scroll gesture event, propagating down to child frames
  // as necessary.
  WebInputEventResult HandleGestureScrollEvent(const WebGestureEvent&);
  WebInputEventResult HandleGestureScrollEnd(const WebGestureEvent&);
  bool IsScrollbarHandlingGestures() const;

  bool BestClickableNodeForHitTestResult(const HitTestResult&,
                                         IntPoint& target_point,
                                         Node*& target_node);
  bool BestContextMenuNodeForHitTestResult(const HitTestResult&,
                                           IntPoint& target_point,
                                           Node*& target_node);
  // FIXME: This doesn't appear to be used outside tests anymore, what path are
  // we using now and is it tested?
  bool BestZoomableAreaForTouchPoint(const IntPoint& touch_center,
                                     const IntSize& touch_radius,
                                     IntRect& target_area,
                                     Node*& target_node);

  WebInputEventResult SendContextMenuEvent(
      const WebMouseEvent&,
      Node* override_target_node = nullptr);
  WebInputEventResult ShowNonLocatedContextMenu(
      Element* override_target_element = nullptr,
      WebMenuSourceType = kMenuSourceNone);

  // Returns whether pointerId is active or not
  bool IsPointerEventActive(int);

  void SetPointerCapture(int, EventTarget*);
  void ReleasePointerCapture(int, EventTarget*);
  void ReleaseMousePointerCapture();
  bool HasPointerCapture(int, const EventTarget*) const;
  bool HasProcessedPointerCapture(int, const EventTarget*) const;
  void ProcessPendingPointerCaptureForPointerLock(const WebMouseEvent&);

  void ElementRemoved(EventTarget*);

  void SetMouseDownMayStartAutoscroll();

  bool HandleAccessKey(const WebKeyboardEvent&);
  WebInputEventResult KeyEvent(const WebKeyboardEvent&);
  void DefaultKeyboardEventHandler(KeyboardEvent*);

  bool HandleTextInputEvent(const String& text,
                            Event* underlying_event = nullptr,
                            TextEventInputType = kTextEventInputKeyboard);
  void DefaultTextInputEventHandler(TextEvent*);

  void DragSourceEndedAt(const WebMouseEvent&, DragOperation);

  void CapsLockStateMayHaveChanged();  // Only called by FrameSelection

  WebInputEventResult HandleTouchEvent(
      const WebTouchEvent&,
      const Vector<WebTouchEvent>& coalesced_events);

  bool UseHandCursor(Node*, bool is_over_link);

  void NotifyElementActivated();

  RefPtr<UserGestureToken> TakeLastMouseDownGestureToken() {
    return std::move(last_mouse_down_user_gesture_token_);
  }

  SelectionController& GetSelectionController() const {
    return *selection_controller_;
  }

  // FIXME(nzolghadr): This function is technically a private function of
  // EventHandler class. Making it public temporary to make it possible to
  // move some code around in the refactoring process.
  // Performs a chaining logical scroll, within a *single* frame, starting
  // from either a provided starting node or a default based on the focused or
  // most recently clicked node, falling back to the frame.
  // Returns true if the scroll was consumed.
  // direction - The logical direction to scroll in. This will be converted to
  //             a physical direction for each LayoutBox we try to scroll
  //             based on that box's writing mode.
  // granularity - The units that the  scroll delta parameter is in.
  // startNode - Optional. If provided, start chaining from the given node.
  //             If not, use the current focus or last clicked node.
  bool LogicalScroll(ScrollDirection,
                     ScrollGranularity,
                     Node* start_node = nullptr);

  bool IsTouchPointerIdActiveOnFrame(int, LocalFrame*) const;

  // Clears drag target and related states. It is called when drag is done or
  // canceled.
  void ClearDragState();

 private:
  enum NoCursorChangeType { kNoCursorChange };

  class OptionalCursor {
   public:
    OptionalCursor(NoCursorChangeType) : is_cursor_change_(false) {}
    OptionalCursor(const Cursor& cursor)
        : is_cursor_change_(true), cursor_(cursor) {}

    bool IsCursorChange() const { return is_cursor_change_; }
    const Cursor& GetCursor() const {
      DCHECK(is_cursor_change_);
      return cursor_;
    }

   private:
    bool is_cursor_change_;
    Cursor cursor_;
  };

  WebInputEventResult HandleMouseMoveOrLeaveEvent(
      const WebMouseEvent&,
      const Vector<WebMouseEvent>&,
      HitTestResult* hovered_node = nullptr,
      bool only_update_scrollbars = false,
      bool force_leave = false);

  void ApplyTouchAdjustment(WebGestureEvent*, HitTestResult*);
  WebInputEventResult HandleGestureTapDown(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureTap(const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongPress(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongTap(
      const GestureEventWithHitTestResults&);

  void UpdateGestureTargetNodeForMouseEvent(
      const GestureEventWithHitTestResults&);

  bool ShouldApplyTouchAdjustment(const WebGestureEvent&) const;

  bool IsSelectingLink(const HitTestResult&);
  bool ShouldShowIBeamForNode(const Node*, const HitTestResult&);
  bool ShouldShowResizeForNode(const Node*, const HitTestResult&);
  OptionalCursor SelectCursor(const HitTestResult&);
  OptionalCursor SelectAutoCursor(const HitTestResult&,
                                  Node*,
                                  const Cursor& i_beam);

  void HoverTimerFired(TimerBase*);
  void CursorUpdateTimerFired(TimerBase*);
  void ActiveIntervalTimerFired(TimerBase*);

  void UpdateCursor();

  ScrollableArea* AssociatedScrollableArea(const PaintLayer*) const;

  Node* UpdateMouseEventTargetNode(Node*);

  // Dispatches ME after corresponding PE provided the PE has not been canceled.
  // The eventType arg must be a mouse event that can be gated though a
  // preventDefaulted pointerdown (i.e., one of
  // {mousedown, mousemove, mouseup}).
  // TODO(mustaq): Can we avoid the clickCount param, instead use
  // WebmMouseEvent's count?
  //     Same applied to dispatchMouseEvent() above.
  WebInputEventResult UpdatePointerTargetAndDispatchEvents(
      const AtomicString& mouse_event_type,
      Node* target,
      const String& canvas_region_id,
      const WebMouseEvent&,
      const Vector<WebMouseEvent>& coalesced_events);

  WebInputEventResult PassMousePressEventToSubframe(
      MouseEventWithHitTestResults&,
      LocalFrame* subframe);
  WebInputEventResult PassMouseMoveEventToSubframe(
      MouseEventWithHitTestResults&,
      const Vector<WebMouseEvent>&,
      LocalFrame* subframe,
      HitTestResult* hovered_node = nullptr);
  WebInputEventResult PassMouseReleaseEventToSubframe(
      MouseEventWithHitTestResults&,
      LocalFrame* subframe);

  bool PassMousePressEventToScrollbar(MouseEventWithHitTestResults&);

  void DefaultSpaceEventHandler(KeyboardEvent*);
  void DefaultBackspaceEventHandler(KeyboardEvent*);
  void DefaultTabEventHandler(KeyboardEvent*);
  void DefaultEscapeEventHandler(KeyboardEvent*);
  void DefaultArrowEventHandler(WebFocusType, KeyboardEvent*);

  void UpdateLastScrollbarUnderMouse(Scrollbar*, bool);

  WebInputEventResult HandleGestureShowPress();

  bool ShouldBrowserControlsConsumeScroll(FloatSize) const;

  bool RootFrameTouchPointerActiveInCurrentFrame(int pointer_id) const;

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |EventHandler::clear()|.

  const Member<LocalFrame> frame_;

  const Member<SelectionController> selection_controller_;

  TaskRunnerTimer<EventHandler> hover_timer_;

  // TODO(rbyers): Mouse cursor update is page-wide, not per-frame.  Page-wide
  // state should move out of EventHandler to a new PageEventHandler class.
  // crbug.com/449649
  TaskRunnerTimer<EventHandler> cursor_update_timer_;

  Member<Node> capturing_mouse_events_node_;
  bool event_handler_will_reset_capturing_mouse_events_node_;

  Member<LocalFrame> last_mouse_move_event_subframe_;
  Member<Scrollbar> last_scrollbar_under_mouse_;

  Member<Node> drag_target_;
  bool should_only_fire_drag_over_event_;

  Member<HTMLFrameSetElement> frame_set_being_resized_;

  RefPtr<UserGestureToken> last_mouse_down_user_gesture_token_;

  Member<ScrollManager> scroll_manager_;
  Member<MouseEventManager> mouse_event_manager_;
  Member<MouseWheelEventManager> mouse_wheel_event_manager_;
  Member<KeyboardEventManager> keyboard_event_manager_;
  Member<PointerEventManager> pointer_event_manager_;
  Member<GestureManager> gesture_manager_;

  double max_mouse_moved_duration_;

  bool long_tap_should_invoke_context_menu_;

  TaskRunnerTimer<EventHandler> active_interval_timer_;
  double last_show_press_timestamp_;
  Member<Element> last_deferred_tap_element_;

  // Set on GestureTapDown if the |pointerdown| event corresponding to the
  // triggering |touchstart| event was canceled. This suppresses mouse event
  // firing for the current gesture sequence (i.e. until next GestureTapDown).
  bool suppress_mouse_events_from_gestures_;

  // ShouldShowIBeamForNode's unit tests:
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest, HitOnNothingDoesNotShowIBeam);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest, HitOnTextShowsIBeam);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest,
                           HitOnUserSelectNoneDoesNotShowIBeam);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest, ChildCanOverrideUserSelectNone);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest,
                           ShadowChildCanOverrideUserSelectNone);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest, ChildCanOverrideUserSelectText);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest,
                           ShadowChildCanOverrideUserSelectText);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest, InputFieldsCanStartSelection);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest, ImagesCannotStartSelection);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest, AnchorTextCannotStartSelection);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest,
                           EditableAnchorTextCanStartSelection);
  FRIEND_TEST_ALL_PREFIXES(EventHandlerTest,
                           ReadOnlyInputDoesNotInheritUserSelect);
};

}  // namespace blink

#endif  // EventHandler_h
