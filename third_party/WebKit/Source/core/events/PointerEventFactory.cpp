// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEventFactory.h"

#include "core/frame/LocalFrameView.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

namespace {

inline int ToInt(WebPointerProperties::PointerType t) {
  return static_cast<int>(t);
}

const char* PointerTypeNameForWebPointPointerType(
    WebPointerProperties::PointerType type) {
  // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
  switch (type) {
    case WebPointerProperties::PointerType::kUnknown:
      return "";
    case WebPointerProperties::PointerType::kTouch:
      return "touch";
    case WebPointerProperties::PointerType::kPen:
      return "pen";
    case WebPointerProperties::PointerType::kMouse:
      return "mouse";
    default:
      NOTREACHED();
      return "";
  }
}

const AtomicString& PointerEventNameForMouseEventName(
    const AtomicString& mouse_event_name) {
#define RETURN_CORRESPONDING_PE_NAME(eventSuffix)               \
  if (mouse_event_name == EventTypeNames::mouse##eventSuffix) { \
    return EventTypeNames::pointer##eventSuffix;                \
  }

  RETURN_CORRESPONDING_PE_NAME(down);
  RETURN_CORRESPONDING_PE_NAME(enter);
  RETURN_CORRESPONDING_PE_NAME(leave);
  RETURN_CORRESPONDING_PE_NAME(move);
  RETURN_CORRESPONDING_PE_NAME(out);
  RETURN_CORRESPONDING_PE_NAME(over);
  RETURN_CORRESPONDING_PE_NAME(up);

#undef RETURN_CORRESPONDING_PE_NAME

  NOTREACHED();
  return g_empty_atom;
}

unsigned short ButtonToButtonsBitfield(WebPointerProperties::Button button) {
#define CASE_BUTTON_TO_BUTTONS(enumLabel)       \
  case WebPointerProperties::Button::enumLabel: \
    return static_cast<unsigned short>(WebPointerProperties::Buttons::enumLabel)

  switch (button) {
    CASE_BUTTON_TO_BUTTONS(kNoButton);
    CASE_BUTTON_TO_BUTTONS(kLeft);
    CASE_BUTTON_TO_BUTTONS(kRight);
    CASE_BUTTON_TO_BUTTONS(kMiddle);
    CASE_BUTTON_TO_BUTTONS(kBack);
    CASE_BUTTON_TO_BUTTONS(kForward);
    CASE_BUTTON_TO_BUTTONS(kEraser);
  }

#undef CASE_BUTTON_TO_BUTTONS

  NOTREACHED();
  return 0;
}

const AtomicString& PointerEventNameForTouchPointState(
    WebTouchPoint::State state) {
  switch (state) {
    case WebTouchPoint::kStateReleased:
      return EventTypeNames::pointerup;
    case WebTouchPoint::kStateCancelled:
      return EventTypeNames::pointercancel;
    case WebTouchPoint::kStatePressed:
      return EventTypeNames::pointerdown;
    case WebTouchPoint::kStateMoved:
      return EventTypeNames::pointermove;
    case WebTouchPoint::kStateStationary:
    // Fall through to default
    default:
      NOTREACHED();
      return g_empty_atom;
  }
}

float GetPointerEventPressure(float force, int buttons) {
  if (!buttons)
    return 0;
  if (std::isnan(force))
    return 0.5;
  return force;
}

void UpdateTouchPointerEventInit(const WebTouchPoint& touch_point,
                                 LocalFrame* target_frame,
                                 PointerEventInit* pointer_event_init) {
  // This function should not update attributes like pointerId, isPrimary,
  // and pointerType which is the same among the coalesced events and the
  // dispatched event.

  if (target_frame) {
    FloatPoint page_point = target_frame->View()->RootFrameToContents(
        touch_point.PositionInWidget());
    float scale_factor = 1.0f / target_frame->PageZoomFactor();
    FloatPoint scroll_position(target_frame->View()->GetScrollOffset());
    FloatPoint client_point = page_point.ScaledBy(scale_factor);
    client_point.MoveBy(scroll_position.ScaledBy(-scale_factor));

    pointer_event_init->setClientX(client_point.X());
    pointer_event_init->setClientY(client_point.Y());

    if (touch_point.state == WebTouchPoint::kStateMoved) {
      pointer_event_init->setMovementX(touch_point.movement_x);
      pointer_event_init->setMovementY(touch_point.movement_y);
    }

    FloatSize point_radius =
        FloatSize(touch_point.radius_x, touch_point.radius_y)
            .ScaledBy(scale_factor);
    pointer_event_init->setWidth(point_radius.Width());
    pointer_event_init->setHeight(point_radius.Height());
  }

  pointer_event_init->setScreenX(touch_point.PositionInScreen().x);
  pointer_event_init->setScreenY(touch_point.PositionInScreen().y);
  pointer_event_init->setPressure(GetPointerEventPressure(
      touch_point.force, pointer_event_init->buttons()));
  pointer_event_init->setTiltX(touch_point.tilt_x);
  pointer_event_init->setTiltY(touch_point.tilt_y);
  pointer_event_init->setTangentialPressure(touch_point.tangential_pressure);
  pointer_event_init->setTwist(touch_point.twist);
}

void UpdateMousePointerEventInit(const WebMouseEvent& mouse_event,
                                 LocalDOMWindow* view,
                                 PointerEventInit* pointer_event_init) {
  // This function should not update attributes like pointerId, isPrimary,
  // and pointerType which is the same among the coalesced events and the
  // dispatched event.

  pointer_event_init->setScreenX(mouse_event.PositionInScreen().x);
  pointer_event_init->setScreenY(mouse_event.PositionInScreen().y);

  IntPoint location_in_frame_zoomed;
  if (view && view->GetFrame() && view->GetFrame()->View()) {
    LocalFrame* frame = view->GetFrame();
    LocalFrameView* frame_view = frame->View();
    IntPoint location_in_contents = frame_view->RootFrameToContents(
        FlooredIntPoint(mouse_event.PositionInRootFrame()));
    location_in_frame_zoomed =
        frame_view->ContentsToFrame(location_in_contents);
    float scale_factor = 1 / frame->PageZoomFactor();
    location_in_frame_zoomed.Scale(scale_factor, scale_factor);
  }

  pointer_event_init->setClientX(location_in_frame_zoomed.X());
  pointer_event_init->setClientY(location_in_frame_zoomed.Y());

  pointer_event_init->setPressure(GetPointerEventPressure(
      mouse_event.force, pointer_event_init->buttons()));
  pointer_event_init->setTiltX(mouse_event.tilt_x);
  pointer_event_init->setTiltY(mouse_event.tilt_y);
  pointer_event_init->setTangentialPressure(mouse_event.tangential_pressure);
  pointer_event_init->setTwist(mouse_event.twist);

  IntPoint movement = FlooredIntPoint(mouse_event.MovementInRootFrame());
  pointer_event_init->setMovementX(movement.X());
  pointer_event_init->setMovementY(movement.Y());
}

}  // namespace

const int PointerEventFactory::kInvalidId = 0;

// Mouse id is 1 to behave the same as MS Edge for compatibility reasons.
const int PointerEventFactory::kMouseId = 1;

void PointerEventFactory::SetIdTypeButtons(
    PointerEventInit& pointer_event_init,
    const WebPointerProperties& pointer_properties,
    unsigned buttons) {
  WebPointerProperties::PointerType pointer_type =
      pointer_properties.pointer_type;
  // Tweak the |buttons| to reflect pen eraser mode only if the pen is in
  // active buttons state w/o even considering the eraser button.
  // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
  if (pointer_type == WebPointerProperties::PointerType::kEraser) {
    if (buttons != 0) {
      buttons |= static_cast<unsigned>(WebPointerProperties::Buttons::kEraser);
      buttons &= ~static_cast<unsigned>(WebPointerProperties::Buttons::kLeft);
    }
    pointer_type = WebPointerProperties::PointerType::kPen;
  }
  pointer_event_init.setButtons(buttons);

  const IncomingId incoming_id(pointer_type, pointer_properties.id);
  int pointer_id = AddIdAndActiveButtons(incoming_id, buttons != 0);
  pointer_event_init.setPointerId(pointer_id);
  pointer_event_init.setPointerType(
      PointerTypeNameForWebPointPointerType(pointer_type));
  pointer_event_init.setIsPrimary(IsPrimary(pointer_id));
}

void PointerEventFactory::SetEventSpecificFields(
    PointerEventInit& pointer_event_init,
    const AtomicString& type) {
  pointer_event_init.setBubbles(type != EventTypeNames::pointerenter &&
                                type != EventTypeNames::pointerleave);
  pointer_event_init.setCancelable(type != EventTypeNames::pointerenter &&
                                   type != EventTypeNames::pointerleave &&
                                   type != EventTypeNames::pointercancel &&
                                   type != EventTypeNames::gotpointercapture &&
                                   type != EventTypeNames::lostpointercapture);

  pointer_event_init.setComposed(true);
  pointer_event_init.setDetail(0);
}

PointerEvent* PointerEventFactory::Create(
    const AtomicString& mouse_event_name,
    const WebMouseEvent& mouse_event,
    const Vector<WebMouseEvent>& coalesced_mouse_events,
    LocalDOMWindow* view) {
  DCHECK(mouse_event_name == EventTypeNames::mousemove ||
         mouse_event_name == EventTypeNames::mousedown ||
         mouse_event_name == EventTypeNames::mouseup);

  AtomicString pointer_event_name =
      PointerEventNameForMouseEventName(mouse_event_name);

  unsigned buttons = MouseEvent::WebInputEventModifiersToButtons(
      static_cast<WebInputEvent::Modifiers>(mouse_event.GetModifiers()));
  PointerEventInit pointer_event_init;

  SetIdTypeButtons(pointer_event_init, mouse_event, buttons);
  SetEventSpecificFields(pointer_event_init, pointer_event_name);

  if (pointer_event_name == EventTypeNames::pointerdown ||
      pointer_event_name == EventTypeNames::pointerup) {
    WebPointerProperties::Button button = mouse_event.button;
    // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
    if (mouse_event.pointer_type ==
            WebPointerProperties::PointerType::kEraser &&
        button == WebPointerProperties::Button::kLeft)
      button = WebPointerProperties::Button::kEraser;
    pointer_event_init.setButton(static_cast<int>(button));
  } else {
    DCHECK(pointer_event_name == EventTypeNames::pointermove);
    pointer_event_init.setButton(
        static_cast<int>(WebPointerProperties::Button::kNoButton));
  }

  UIEventWithKeyState::SetFromWebInputEventModifiers(
      pointer_event_init,
      static_cast<WebInputEvent::Modifiers>(mouse_event.GetModifiers()));

  // Make sure chorded buttons fire pointermove instead of pointerup/down.
  if ((pointer_event_name == EventTypeNames::pointerdown &&
       (buttons & ~ButtonToButtonsBitfield(mouse_event.button)) != 0) ||
      (pointer_event_name == EventTypeNames::pointerup && buttons != 0))
    pointer_event_name = EventTypeNames::pointermove;

  pointer_event_init.setView(view);

  UpdateMousePointerEventInit(mouse_event, view, &pointer_event_init);

  // Create coalesced events init structure only for pointermove.
  if (pointer_event_name == EventTypeNames::pointermove) {
    HeapVector<Member<PointerEvent>> coalesced_pointer_events;
    for (const auto& coalesced_mouse_event : coalesced_mouse_events) {
      // TODO(crbug.com/733774): Disabled the DCHECK on the id of mouse events
      // because it failed on some versions of Mac OS.
      // DCHECK_EQ(mouse_event.id, coalesced_mouse_event.id);

      DCHECK_EQ(mouse_event.pointer_type, coalesced_mouse_event.pointer_type);
      PointerEventInit coalesced_event_init = pointer_event_init;
      coalesced_event_init.setCancelable(false);
      coalesced_event_init.setBubbles(false);
      UpdateMousePointerEventInit(coalesced_mouse_event, view,
                                  &coalesced_event_init);
      PointerEvent* event = PointerEvent::Create(
          pointer_event_name, coalesced_event_init,
          TimeTicks::FromSeconds(coalesced_mouse_event.TimeStampSeconds()));
      // Set the trusted flag for the coalesced events at the creation time
      // as oppose to the normal events which is done at the dispatch time. This
      // is because we don't want to go over all the coalesced events at every
      // dispatch and add the implementation complexity while it has no sensible
      // usecase at this time.
      event->SetTrusted(true);
      coalesced_pointer_events.push_back(event);
    }
    pointer_event_init.setCoalescedEvents(coalesced_pointer_events);
  }

  return PointerEvent::Create(
      pointer_event_name, pointer_event_init,
      TimeTicks::FromSeconds(mouse_event.TimeStampSeconds()));
}

PointerEvent* PointerEventFactory::Create(
    const WebTouchPoint& touch_point,
    const Vector<std::pair<WebTouchPoint, TimeTicks>>& coalesced_points,
    WebInputEvent::Modifiers modifiers,
    TimeTicks event_platform_time_stamp,
    LocalFrame* target_frame,
    DOMWindow* view) {
  const WebTouchPoint::State point_state = touch_point.state;
  const AtomicString& type =
      PointerEventNameForTouchPointState(touch_point.state);

  bool pointer_released_or_cancelled =
      point_state == WebTouchPoint::State::kStateReleased ||
      point_state == WebTouchPoint::State::kStateCancelled;
  bool pointer_pressed_or_released =
      point_state == WebTouchPoint::State::kStatePressed ||
      point_state == WebTouchPoint::State::kStateReleased;

  PointerEventInit pointer_event_init;

  SetIdTypeButtons(pointer_event_init, touch_point,
                   pointer_released_or_cancelled ? 0 : 1);
  pointer_event_init.setButton(static_cast<int>(
      pointer_pressed_or_released ? WebPointerProperties::Button::kLeft
                                  : WebPointerProperties::Button::kNoButton));

  pointer_event_init.setView(view);
  UpdateTouchPointerEventInit(touch_point, target_frame, &pointer_event_init);

  UIEventWithKeyState::SetFromWebInputEventModifiers(pointer_event_init,
                                                     modifiers);

  SetEventSpecificFields(pointer_event_init, type);

  if (type == EventTypeNames::pointermove) {
    HeapVector<Member<PointerEvent>> coalesced_pointer_events;
    for (const auto& coalesced_touch_point : coalesced_points) {
      const auto& coalesced_point = coalesced_touch_point.first;
      const auto& coalesced_point_time_stamp = coalesced_touch_point.second;
      DCHECK_EQ(touch_point.state, coalesced_point.state);
      DCHECK_EQ(touch_point.id, coalesced_point.id);
      DCHECK_EQ(touch_point.pointer_type, coalesced_point.pointer_type);
      PointerEventInit coalesced_event_init = pointer_event_init;
      coalesced_event_init.setCancelable(false);
      coalesced_event_init.setBubbles(false);
      UpdateTouchPointerEventInit(coalesced_point, target_frame,
                                  &coalesced_event_init);
      PointerEvent* event = PointerEvent::Create(type, coalesced_event_init,
                                                 coalesced_point_time_stamp);
      // Set the trusted flag for the coalesced events at the creation time
      // as oppose to the normal events which is done at the dispatch time. This
      // is because we don't want to go over all the coalesced events at every
      // dispatch and add the implementation complexity while it has no sensible
      // usecase at this time.
      event->SetTrusted(true);
      coalesced_pointer_events.push_back(event);
    }
    pointer_event_init.setCoalescedEvents(coalesced_pointer_events);
  }

  return PointerEvent::Create(type, pointer_event_init,
                              event_platform_time_stamp);
}

PointerEvent* PointerEventFactory::CreatePointerCancelEvent(
    const WebPointerEvent& event) {
  DCHECK_EQ(event.GetType(), WebInputEvent::Type::kPointerCancel);
  int pointer_id = GetPointerEventId(event);

  return CreatePointerCancelEvent(
      pointer_id, event.pointer_type,
      TimeTicks::FromSeconds(event.TimeStampSeconds()));
}

PointerEvent* PointerEventFactory::CreatePointerCancelEvent(
    const int pointer_id,
    const WebPointerProperties::PointerType pointer_type,
    TimeTicks platfrom_time_stamp) {
  DCHECK(pointer_id_mapping_.Contains(pointer_id));
  pointer_id_mapping_.Set(
      pointer_id,
      PointerAttributes(pointer_id_mapping_.at(pointer_id).incoming_id, false));

  PointerEventInit pointer_event_init;

  pointer_event_init.setPointerId(pointer_id);
  pointer_event_init.setPointerType(
      PointerTypeNameForWebPointPointerType(pointer_type));
  pointer_event_init.setIsPrimary(IsPrimary(pointer_id));

  SetEventSpecificFields(pointer_event_init, EventTypeNames::pointercancel);

  return PointerEvent::Create(EventTypeNames::pointercancel, pointer_event_init,
                              platfrom_time_stamp);
}

PointerEvent* PointerEventFactory::CreatePointerEventFrom(
    PointerEvent* pointer_event,
    const AtomicString& type,
    EventTarget* related_target) {
  PointerEventInit pointer_event_init;

  pointer_event_init.setPointerId(pointer_event->pointerId());
  pointer_event_init.setPointerType(pointer_event->pointerType());
  pointer_event_init.setIsPrimary(pointer_event->isPrimary());
  pointer_event_init.setWidth(pointer_event->width());
  pointer_event_init.setHeight(pointer_event->height());
  pointer_event_init.setScreenX(pointer_event->screenX());
  pointer_event_init.setScreenY(pointer_event->screenY());
  pointer_event_init.setClientX(pointer_event->clientX());
  pointer_event_init.setClientY(pointer_event->clientY());
  pointer_event_init.setButton(pointer_event->button());
  pointer_event_init.setButtons(pointer_event->buttons());
  pointer_event_init.setPressure(pointer_event->pressure());
  pointer_event_init.setTiltX(pointer_event->tiltX());
  pointer_event_init.setTiltY(pointer_event->tiltY());
  pointer_event_init.setTangentialPressure(pointer_event->tangentialPressure());
  pointer_event_init.setTwist(pointer_event->twist());
  pointer_event_init.setView(pointer_event->view());

  SetEventSpecificFields(pointer_event_init, type);

  if (related_target)
    pointer_event_init.setRelatedTarget(related_target);

  return PointerEvent::Create(type, pointer_event_init,
                              pointer_event->PlatformTimeStamp());
}

PointerEvent* PointerEventFactory::CreatePointerCaptureEvent(
    PointerEvent* pointer_event,
    const AtomicString& type) {
  DCHECK(type == EventTypeNames::gotpointercapture ||
         type == EventTypeNames::lostpointercapture);

  return CreatePointerEventFrom(pointer_event, type,
                                pointer_event->relatedTarget());
}

PointerEvent* PointerEventFactory::CreatePointerBoundaryEvent(
    PointerEvent* pointer_event,
    const AtomicString& type,
    EventTarget* related_target) {
  DCHECK(type == EventTypeNames::pointerout ||
         type == EventTypeNames::pointerleave ||
         type == EventTypeNames::pointerover ||
         type == EventTypeNames::pointerenter);

  return CreatePointerEventFrom(pointer_event, type, related_target);
}

PointerEventFactory::PointerEventFactory() {
  Clear();
}

PointerEventFactory::~PointerEventFactory() {
  Clear();
}

void PointerEventFactory::Clear() {
  for (int type = 0;
       type <= ToInt(WebPointerProperties::PointerType::kLastEntry); type++) {
    primary_id_[type] = PointerEventFactory::kInvalidId;
    id_count_[type] = 0;
  }
  pointer_incoming_id_mapping_.clear();
  pointer_id_mapping_.clear();

  // Always add mouse pointer in initialization and never remove it.
  // No need to add it to m_pointerIncomingIdMapping as it is not going to be
  // used with the existing APIs
  primary_id_[ToInt(WebPointerProperties::PointerType::kMouse)] = kMouseId;
  pointer_id_mapping_.insert(
      kMouseId,
      PointerAttributes(
          IncomingId(WebPointerProperties::PointerType::kMouse, 0), 0));

  current_id_ = PointerEventFactory::kMouseId + 1;
}

int PointerEventFactory::AddIdAndActiveButtons(const IncomingId p,
                                               bool is_active_buttons) {
  // Do not add extra mouse pointer as it was added in initialization
  if (p.GetPointerType() == WebPointerProperties::PointerType::kMouse) {
    pointer_id_mapping_.Set(kMouseId, PointerAttributes(p, is_active_buttons));
    return kMouseId;
  }

  if (pointer_incoming_id_mapping_.Contains(p)) {
    int mapped_id = pointer_incoming_id_mapping_.at(p);
    pointer_id_mapping_.Set(mapped_id, PointerAttributes(p, is_active_buttons));
    return mapped_id;
  }
  int type_int = p.PointerTypeInt();
  // We do not handle the overflow of m_currentId as it should be very rare
  int mapped_id = current_id_++;
  if (!id_count_[type_int])
    primary_id_[type_int] = mapped_id;
  id_count_[type_int]++;
  pointer_incoming_id_mapping_.insert(p, mapped_id);
  pointer_id_mapping_.insert(mapped_id,
                             PointerAttributes(p, is_active_buttons));
  return mapped_id;
}

bool PointerEventFactory::Remove(const int mapped_id) {
  // Do not remove mouse pointer id as it should always be there
  if (mapped_id == kMouseId || !pointer_id_mapping_.Contains(mapped_id))
    return false;

  IncomingId p = pointer_id_mapping_.at(mapped_id).incoming_id;
  int type_int = p.PointerTypeInt();
  pointer_id_mapping_.erase(mapped_id);
  pointer_incoming_id_mapping_.erase(p);
  if (primary_id_[type_int] == mapped_id)
    primary_id_[type_int] = PointerEventFactory::kInvalidId;
  id_count_[type_int]--;
  return true;
}

// This function does not work with pointer type of eraser, because we save
// them as pen type in the pointer id map.
Vector<int> PointerEventFactory::GetPointerIdsOfType(
    WebPointerProperties::PointerType pointer_type) const {
  Vector<int> mapped_ids;

  for (auto iter = pointer_id_mapping_.begin();
       iter != pointer_id_mapping_.end(); ++iter) {
    int mapped_id = iter->key;
    if (iter->value.incoming_id.GetPointerType() == pointer_type)
      mapped_ids.push_back(mapped_id);
  }

  // Sorting for a predictable ordering.
  std::sort(mapped_ids.begin(), mapped_ids.end());
  return mapped_ids;
}

bool PointerEventFactory::IsPrimary(int mapped_id) const {
  if (!pointer_id_mapping_.Contains(mapped_id))
    return false;

  IncomingId p = pointer_id_mapping_.at(mapped_id).incoming_id;
  return primary_id_[p.PointerTypeInt()] == mapped_id;
}

bool PointerEventFactory::IsActive(const int pointer_id) const {
  return pointer_id_mapping_.Contains(pointer_id);
}

bool PointerEventFactory::IsActiveButtonsState(const int pointer_id) const {
  return pointer_id_mapping_.Contains(pointer_id) &&
         pointer_id_mapping_.at(pointer_id).is_active_buttons;
}

WebPointerProperties::PointerType PointerEventFactory::GetPointerType(
    int pointer_id) const {
  if (!IsActive(pointer_id))
    return WebPointerProperties::PointerType::kUnknown;
  return pointer_id_mapping_.at(pointer_id).incoming_id.GetPointerType();
}

int PointerEventFactory::GetPointerEventId(
    const WebPointerProperties& properties) const {
  if (properties.pointer_type == WebPointerProperties::PointerType::kMouse)
    return PointerEventFactory::kMouseId;
  IncomingId id(properties.pointer_type, properties.id);
  if (pointer_incoming_id_mapping_.Contains(id))
    return pointer_incoming_id_mapping_.at(id);
  return PointerEventFactory::kInvalidId;
}

}  // namespace blink
