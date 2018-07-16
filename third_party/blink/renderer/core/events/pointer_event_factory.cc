// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pointer_event_factory.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"

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

const AtomicString& PointerEventNameForEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kPointerDown:
      return EventTypeNames::pointerdown;
    case WebInputEvent::kPointerUp:
      return EventTypeNames::pointerup;
    case WebInputEvent::kPointerMove:
      return EventTypeNames::pointermove;
    case WebInputEvent::kPointerCancel:
      return EventTypeNames::pointercancel;
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

void UpdateCommonPointerEventInit(const WebPointerEvent& web_pointer_event,
                                  LocalDOMWindow* dom_window,
                                  PointerEventInit* pointer_event_init) {
  // This function should not update attributes like pointerId, isPrimary,
  // and pointerType which is the same among the coalesced events and the
  // dispatched event.

  WebPointerEvent web_pointer_event_in_root_frame =
      web_pointer_event.WebPointerEventInRootFrame();

  MouseEvent::SetCoordinatesFromWebPointerProperties(
      web_pointer_event_in_root_frame, dom_window, *pointer_event_init);
  // If width/height is unknown we let PointerEventInit set it to 1.
  // See https://w3c.github.io/pointerevents/#dom-pointerevent-width
  if (web_pointer_event_in_root_frame.HasWidth() &&
      web_pointer_event_in_root_frame.HasHeight()) {
    float scale_factor = 1.0f;
    if (dom_window && dom_window->GetFrame())
      scale_factor = 1.0f / dom_window->GetFrame()->PageZoomFactor();

    FloatSize point_shape = FloatSize(web_pointer_event_in_root_frame.width,
                                      web_pointer_event_in_root_frame.height)
                                .ScaledBy(scale_factor);
    pointer_event_init->setWidth(point_shape.Width());
    pointer_event_init->setHeight(point_shape.Height());
  }
  pointer_event_init->setPressure(GetPointerEventPressure(
      web_pointer_event.force, pointer_event_init->buttons()));
  pointer_event_init->setTiltX(web_pointer_event.tilt_x);
  pointer_event_init->setTiltY(web_pointer_event.tilt_y);
  pointer_event_init->setTangentialPressure(
      web_pointer_event.tangential_pressure);
  pointer_event_init->setTwist(web_pointer_event.twist);
}

}  // namespace

const int PointerEventFactory::kInvalidId = 0;

// Mouse id is 1 to behave the same as MS Edge for compatibility reasons.
const int PointerEventFactory::kMouseId = 1;

void PointerEventFactory::SetIdTypeButtons(
    PointerEventInit& pointer_event_init,
    const WebPointerEvent& web_pointer_event) {
  WebPointerProperties::PointerType pointer_type =
      web_pointer_event.pointer_type;
  unsigned buttons;
  if (web_pointer_event.hovering) {
    buttons = MouseEvent::WebInputEventModifiersToButtons(
        static_cast<WebInputEvent::Modifiers>(
            web_pointer_event.GetModifiers()));
  } else {
    // TODO(crbug.com/816504): This is incorrect as we are assuming pointers
    // that don't hover have no other buttons except left which represents
    // touching the screen. This misconception comes from the touch devices and
    // is not correct for stylus.
    buttons = static_cast<unsigned>(
        (web_pointer_event.GetType() == WebInputEvent::kPointerUp ||
         web_pointer_event.GetType() == WebInputEvent::kPointerCancel)
            ? WebPointerProperties::Buttons::kNoButton
            : WebPointerProperties::Buttons::kLeft);
  }
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

  const IncomingId incoming_id(pointer_type, web_pointer_event.id);
  int pointer_id = AddIdAndActiveButtons(incoming_id, buttons != 0,
                                         web_pointer_event.hovering);
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
    const WebPointerEvent& web_pointer_event,
    const Vector<WebPointerEvent>& coalesced_events,
    LocalDOMWindow* view) {
  const WebInputEvent::Type event_type = web_pointer_event.GetType();
  DCHECK(event_type == WebInputEvent::kPointerDown ||
         event_type == WebInputEvent::kPointerUp ||
         event_type == WebInputEvent::kPointerMove ||
         event_type == WebInputEvent::kPointerCancel);

  PointerEventInit pointer_event_init;

  SetIdTypeButtons(pointer_event_init, web_pointer_event);

  AtomicString type = PointerEventNameForEventType(event_type);
  if (event_type == WebInputEvent::kPointerDown ||
      event_type == WebInputEvent::kPointerUp) {
    WebPointerProperties::Button button = web_pointer_event.button;
    // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
    if (web_pointer_event.pointer_type ==
            WebPointerProperties::PointerType::kEraser &&
        button == WebPointerProperties::Button::kLeft)
      button = WebPointerProperties::Button::kEraser;
    pointer_event_init.setButton(static_cast<int>(button));

    // Make sure chorded buttons fire pointermove instead of pointerup/down.
    if ((event_type == WebInputEvent::kPointerDown &&
         (pointer_event_init.buttons() & ~ButtonToButtonsBitfield(button)) !=
             0) ||
        (event_type == WebInputEvent::kPointerUp &&
         pointer_event_init.buttons() != 0))
      type = EventTypeNames::pointermove;
  } else {
    pointer_event_init.setButton(
        static_cast<int>(WebPointerProperties::Button::kNoButton));
  }

  pointer_event_init.setView(view);
  UpdateCommonPointerEventInit(web_pointer_event, view, &pointer_event_init);

  UIEventWithKeyState::SetFromWebInputEventModifiers(
      pointer_event_init,
      static_cast<WebInputEvent::Modifiers>(web_pointer_event.GetModifiers()));

  SetEventSpecificFields(pointer_event_init, type);

  if (type == EventTypeNames::pointermove) {
    HeapVector<Member<PointerEvent>> coalesced_pointer_events;
    for (const auto& coalesced_event : coalesced_events) {
      DCHECK_EQ(web_pointer_event.id, coalesced_event.id);
      DCHECK_EQ(web_pointer_event.GetType(), coalesced_event.GetType());
      DCHECK_EQ(web_pointer_event.pointer_type, coalesced_event.pointer_type);

      PointerEventInit coalesced_event_init = pointer_event_init;
      coalesced_event_init.setCancelable(false);
      coalesced_event_init.setBubbles(false);
      UpdateCommonPointerEventInit(coalesced_event, view,
                                   &coalesced_event_init);
      PointerEvent* event = PointerEvent::Create(type, coalesced_event_init,
                                                 coalesced_event.TimeStamp());
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
                              web_pointer_event.TimeStamp());
}

PointerEvent* PointerEventFactory::CreatePointerCancelEvent(
    const int pointer_id,
    TimeTicks platfrom_time_stamp) {
  DCHECK(pointer_id_mapping_.Contains(pointer_id));
  pointer_id_mapping_.Set(
      pointer_id,
      PointerAttributes(pointer_id_mapping_.at(pointer_id).incoming_id, false,
                        true));

  PointerEventInit pointer_event_init;

  pointer_event_init.setPointerId(pointer_id);
  pointer_event_init.setPointerType(PointerTypeNameForWebPointPointerType(
      pointer_id_mapping_.at(pointer_id).incoming_id.GetPointerType()));
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
      kMouseId, PointerAttributes(
                    IncomingId(WebPointerProperties::PointerType::kMouse, 0),
                    false, true));

  current_id_ = PointerEventFactory::kMouseId + 1;
}

int PointerEventFactory::AddIdAndActiveButtons(const IncomingId p,
                                               bool is_active_buttons,
                                               bool hovering) {
  // Do not add extra mouse pointer as it was added in initialization
  if (p.GetPointerType() == WebPointerProperties::PointerType::kMouse) {
    pointer_id_mapping_.Set(kMouseId,
                            PointerAttributes(p, is_active_buttons, true));
    return kMouseId;
  }

  if (pointer_incoming_id_mapping_.Contains(p)) {
    int mapped_id = pointer_incoming_id_mapping_.at(p);
    pointer_id_mapping_.Set(mapped_id,
                            PointerAttributes(p, is_active_buttons, hovering));
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
                             PointerAttributes(p, is_active_buttons, hovering));
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

Vector<int> PointerEventFactory::GetPointerIdsOfNonHoveringPointers() const {
  Vector<int> mapped_ids;

  for (auto iter = pointer_id_mapping_.begin();
       iter != pointer_id_mapping_.end(); ++iter) {
    int mapped_id = iter->key;
    if (!iter->value.hovering)
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

bool PointerEventFactory::IsPrimary(
    const WebPointerProperties& properties) const {
  // Mouse event is always primary.
  if (properties.pointer_type == WebPointerProperties::PointerType::kMouse)
    return true;

  // If !id_count, no pointer active, current WebPointerEvent will
  // be primary pointer when added to map.
  if (!id_count_[static_cast<int>(properties.pointer_type)])
    return true;

  int pointer_id = GetPointerEventId(properties);
  return (pointer_id != PointerEventFactory::kInvalidId &&
          IsPrimary(pointer_id));
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
