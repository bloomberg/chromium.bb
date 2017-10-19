// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerEventManager_h
#define PointerEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEvent.h"
#include "core/events/PointerEventFactory.h"
#include "core/input/BoundaryEventDispatcher.h"
#include "core/input/TouchEventManager.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebPointerProperties.h"

namespace blink {

class LocalFrame;
class MouseEventManager;

// This class takes care of dispatching all pointer events and keeps track of
// properties of active pointer events.
class CORE_EXPORT PointerEventManager
    : public GarbageCollectedFinalized<PointerEventManager> {
  WTF_MAKE_NONCOPYABLE(PointerEventManager);

 public:
  PointerEventManager(LocalFrame&, MouseEventManager&);
  void Trace(blink::Visitor*);

  // This is the unified path for handling all input device events. This may
  // cause firing DOM pointerevents, mouseevent, and touch events accordingly.
  // TODO(crbug.com/625841): We need to get all event handling path to go
  // through this function.
  WebInputEventResult HandlePointerEvent(const WebPointerEvent&, Node* target);

  // Sends the mouse pointer events and the boundary events
  // that it may cause. It also sends the compat mouse events
  // and sets the newNodeUnderMouse if the capturing is set
  // in this function.
  WebInputEventResult SendMousePointerEvent(
      Node* target,
      const String& canvas_region_id,
      const AtomicString& type,
      const WebMouseEvent&,
      const Vector<WebMouseEvent>& coalesced_events);

  WebInputEventResult HandleTouchEvents(
      const WebTouchEvent&,
      const Vector<WebTouchEvent>& coalesced_events);

  // Sends boundary events pointerout/leave/over/enter and
  // mouseout/leave/over/enter to the corresponding targets.
  // inside the document. This functions handles the cases that pointer is
  // leaving a frame. Note that normal mouse events (e.g. mousemove/down/up)
  // and their corresponding boundary events will be handled altogether by
  // sendMousePointerEvent function.
  void SendMouseAndPointerBoundaryEvents(Node* entered_node,
                                         const String& canvas_region_id,
                                         const WebMouseEvent&);

  // Resets the internal state of this object.
  void Clear();

  void ElementRemoved(EventTarget*);

  void SetPointerCapture(int, EventTarget*);
  void ReleasePointerCapture(int, EventTarget*);
  void ReleaseMousePointerCapture();

  // See Element::hasPointerCapture(int).
  bool HasPointerCapture(int, const EventTarget*) const;

  // See Element::hasProcessedPointerCapture(int).
  bool HasProcessedPointerCapture(int, const EventTarget*) const;

  bool IsActive(const int) const;

  // Returns whether there is any touch on the screen.
  bool IsAnyTouchActive() const;

  // Returns whether pointerId is for an active touch pointerevent and whether
  // the last event was sent to the given frame.
  bool IsTouchPointerIdActiveOnFrame(int, LocalFrame*) const;

  // Returns true if the primary pointerdown corresponding to the given
  // |uniqueTouchEventId| was canceled. Also drops stale ids from
  // |m_touchIdsForCanceledPointerdowns|.
  bool PrimaryPointerdownCanceled(uint32_t unique_touch_event_id);

  void ProcessPendingPointerCaptureForPointerLock(const WebMouseEvent&);

 private:
  typedef HeapHashMap<int,
                      Member<EventTarget>,
                      WTF::IntHash<int>,
                      WTF::UnsignedWithZeroKeyHashTraits<int>>
      PointerCapturingMap;
  class EventTargetAttributes {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    void Trace(blink::Visitor* visitor) { visitor->Trace(target); }
    Member<EventTarget> target;
    bool has_recieved_over_event;
    EventTargetAttributes() : target(nullptr), has_recieved_over_event(false) {}
    EventTargetAttributes(EventTarget* target, bool has_recieved_over_event)
        : target(target), has_recieved_over_event(has_recieved_over_event) {}
  };

  class PointerEventBoundaryEventDispatcher : public BoundaryEventDispatcher {
    WTF_MAKE_NONCOPYABLE(PointerEventBoundaryEventDispatcher);

   public:
    PointerEventBoundaryEventDispatcher(PointerEventManager*, PointerEvent*);

   protected:
    void DispatchOut(EventTarget*, EventTarget* related_target) override;
    void DispatchOver(EventTarget*, EventTarget* related_target) override;
    void DispatchLeave(EventTarget*,
                       EventTarget* related_target,
                       bool check_for_listener) override;
    void DispatchEnter(EventTarget*,
                       EventTarget* related_target,
                       bool check_for_listener) override;
    AtomicString GetLeaveEvent() override;
    AtomicString GetEnterEvent() override;

   private:
    void Dispatch(EventTarget*,
                  EventTarget* related_target,
                  const AtomicString&,
                  bool check_for_listener);
    Member<PointerEventManager> pointer_event_manager_;
    Member<PointerEvent> pointer_event_;
  };

  // Inhibits firing of touch-type PointerEvents until unblocked by
  // unblockTouchPointers(). Also sends pointercancels for existing touch-type
  // PointerEvents.  See:
  // www.w3.org/TR/pointerevents/#declaring-candidate-regions-for-default-touch-behaviors
  void BlockTouchPointers(TimeTicks platform_time_stamp);

  // Enables firing of touch-type PointerEvents after they were inhibited by
  // blockTouchPointers().
  void UnblockTouchPointers();

  // Returns PointerEventTarget for a WebTouchPoint, hit-testing as necessary.
  EventHandlingUtil::PointerEventTarget ComputePointerEventTarget(
      const WebTouchPoint&);

  // Sends touch pointer events and sets consumed bits in TouchInfo array
  // based on the return value of pointer event handlers.
  void DispatchTouchPointerEvent(
      const WebTouchPoint&,
      const EventHandlingUtil::PointerEventTarget&,
      const Vector<std::pair<WebTouchPoint, TimeTicks>>& coalesced_events,
      WebInputEvent::Modifiers,
      double timestamp,
      uint32_t unique_touch_event_id);

  // Returns whether the event is consumed or not.
  WebInputEventResult SendTouchPointerEvent(EventTarget*, PointerEvent*);

  void SendBoundaryEvents(EventTarget* exited_target,
                          EventTarget* entered_target,
                          PointerEvent*);
  void SetNodeUnderPointer(PointerEvent*, EventTarget*);

  // Processes the assignment of |m_pointerCaptureTarget| from
  // |m_pendingPointerCaptureTarget| and sends the got/lostpointercapture
  // events, as per the spec:
  // https://w3c.github.io/pointerevents/#process-pending-pointer-capture
  void ProcessPendingPointerCapture(PointerEvent*);

  // Processes the capture state of a pointer, updates node under
  // pointer, and sends corresponding boundary events for pointer if
  // setPointerPosition is true. It also sends corresponding boundary events
  // for mouse if sendMouseEvent is true.
  // Returns the target that the pointer event is supposed to be fired at.
  EventTarget* ProcessCaptureAndPositionOfPointerEvent(
      PointerEvent*,
      EventTarget* hit_test_target,
      const String& canvas_region_id = String(),
      const WebMouseEvent* = nullptr);

  void RemoveTargetFromPointerCapturingMapping(PointerCapturingMap&,
                                               const EventTarget*);
  EventTarget* GetEffectiveTargetForPointerEvent(EventTarget*, int);
  EventTarget* GetCapturingNode(int);
  void RemovePointer(PointerEvent*);
  WebInputEventResult DispatchPointerEvent(EventTarget*,
                                           PointerEvent*,
                                           bool check_for_listener = false);
  void ReleasePointerCapture(int);
  // Returns true if capture target and pending capture target were different.
  bool GetPointerCaptureState(int pointer_id,
                              EventTarget** pointer_capture_target,
                              EventTarget** pending_pointer_capture_target);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |PointerEventManager::clear()|.

  const Member<LocalFrame> frame_;

  // Prevents firing mousedown, mousemove & mouseup in-between a canceled
  // pointerdown and next pointerup/pointercancel.
  // See "PREVENT MOUSE EVENT flag" in the spec:
  //   https://w3c.github.io/pointerevents/#compatibility-mapping-with-mouse-events
  bool prevent_mouse_event_for_pointer_type_
      [static_cast<size_t>(WebPointerProperties::PointerType::kLastEntry) + 1];

  // Set upon TouchScrollStarted when sending a pointercancel, prevents PE
  // dispatches for touches until all touch-points become inactive.
  bool in_canceled_state_for_pointer_type_touch_;

  Deque<uint32_t> touch_ids_for_canceled_pointerdowns_;

  // Note that this map keeps track of node under pointer with id=1 as well
  // which might be different than m_nodeUnderMouse in EventHandler. That one
  // keeps track of any compatibility mouse event positions but this map for
  // the pointer with id=1 is only taking care of true mouse related events.
  using NodeUnderPointerMap =
      HeapHashMap<int,
                  EventTargetAttributes,
                  WTF::IntHash<int>,
                  WTF::UnsignedWithZeroKeyHashTraits<int>>;
  NodeUnderPointerMap node_under_pointer_;

  PointerCapturingMap pointer_capture_target_;
  PointerCapturingMap pending_pointer_capture_target_;

  PointerEventFactory pointer_event_factory_;
  Member<TouchEventManager> touch_event_manager_;
  Member<MouseEventManager> mouse_event_manager_;

  // The pointerId of the PointerEvent currently being dispatched within this
  // frame or 0 if none.
  int dispatching_pointer_id_;
};

}  // namespace blink

#endif  // PointerEventManager_h
