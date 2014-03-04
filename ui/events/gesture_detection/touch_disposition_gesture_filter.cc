// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

#include "base/auto_reset.h"
#include "base/logging.h"

namespace ui {
namespace {

// A BitSet32 is used for tracking dropped gesture types.
COMPILE_ASSERT(ET_GESTURE_TYPE_LAST - ET_GESTURE_TYPE_FIRST < 32,
               gesture_type_count_too_large);

GestureEventData CreateGesture(EventType type) {
  return GestureEventData(
      type, base::TimeTicks(), 0, 0, GestureEventData::Details());
}

enum RequiredTouches {
  RT_NONE = 0,
  RT_START = 1 << 0,
  RT_CURRENT = 1 << 1,
};

struct DispositionHandlingInfo {
  // A bitwise-OR of |RequiredTouches|.
  int required_touches;
  EventType antecedent_event_type;

  DispositionHandlingInfo(int required_touches)
      : required_touches(required_touches) {}

  DispositionHandlingInfo(int required_touches,
                          EventType antecedent_event_type)
      : required_touches(required_touches),
        antecedent_event_type(antecedent_event_type) {}
};

DispositionHandlingInfo Info(int required_touches) {
  return DispositionHandlingInfo(required_touches);
}

DispositionHandlingInfo Info(int required_touches,
                             EventType antecedent_event_type) {
  return DispositionHandlingInfo(required_touches, antecedent_event_type);
}

// This approach to disposition handling is described at http://goo.gl/5G8PWJ.
DispositionHandlingInfo GetDispositionHandlingInfo(EventType type) {
  switch (type) {
    case ET_GESTURE_TAP_DOWN:
      return Info(RT_START);
    case ET_GESTURE_TAP_CANCEL:
      return Info(RT_START);
    case ET_GESTURE_SHOW_PRESS:
      return Info(RT_START);
    case ET_GESTURE_LONG_PRESS:
      return Info(RT_START);
    case ET_GESTURE_LONG_TAP:
      return Info(RT_START | RT_CURRENT);
    case ET_GESTURE_TAP:
      return Info(RT_START | RT_CURRENT, ET_GESTURE_TAP_UNCONFIRMED);
    case ET_GESTURE_TAP_UNCONFIRMED:
      return Info(RT_START | RT_CURRENT);
    case ET_GESTURE_DOUBLE_TAP:
      return Info(RT_START | RT_CURRENT, ET_GESTURE_TAP_UNCONFIRMED);
    case ET_GESTURE_SCROLL_BEGIN:
      return Info(RT_START | RT_CURRENT);
    case ET_GESTURE_SCROLL_UPDATE:
      return Info(RT_CURRENT, ET_GESTURE_SCROLL_BEGIN);
    case ET_GESTURE_SCROLL_END:
      return Info(RT_NONE, ET_GESTURE_SCROLL_BEGIN);
    case ET_SCROLL_FLING_START:
      return Info(RT_NONE, ET_GESTURE_SCROLL_BEGIN);
    case ET_SCROLL_FLING_CANCEL:
      return Info(RT_NONE, ET_SCROLL_FLING_START);
    case ET_GESTURE_PINCH_BEGIN:
      return Info(RT_START, ET_GESTURE_SCROLL_BEGIN);
    case ET_GESTURE_PINCH_UPDATE:
      return Info(RT_CURRENT, ET_GESTURE_PINCH_BEGIN);
    case ET_GESTURE_PINCH_END:
      return Info(RT_NONE, ET_GESTURE_PINCH_BEGIN);
    default:
      break;
  }
  NOTREACHED();
  return Info(RT_NONE);
}

int GetGestureTypeIndex(EventType type) {
  return type - ET_GESTURE_TYPE_FIRST;
}

}  // namespace

// TouchDispositionGestureFilter

TouchDispositionGestureFilter::TouchDispositionGestureFilter(
    TouchDispositionGestureFilterClient* client)
    : client_(client),
      needs_tap_ending_event_(false),
      needs_fling_ending_event_(false),
      needs_scroll_ending_event_(false) {
  DCHECK(client_);
}

TouchDispositionGestureFilter::~TouchDispositionGestureFilter() {}

TouchDispositionGestureFilter::PacketResult
TouchDispositionGestureFilter::OnGesturePacket(
    const GestureEventDataPacket& packet) {
  if (packet.gesture_source() == GestureEventDataPacket::UNDEFINED ||
      packet.gesture_source() == GestureEventDataPacket::INVALID)
    return INVALID_PACKET_TYPE;

  if (packet.gesture_source() == GestureEventDataPacket::TOUCH_SEQUENCE_START)
    sequences_.push(GestureSequence());

  if (IsEmpty())
    return INVALID_PACKET_ORDER;

  if (packet.gesture_source() == GestureEventDataPacket::TOUCH_TIMEOUT &&
      Tail().IsEmpty()) {
    // Handle the timeout packet immediately if the packet preceding the timeout
    // has already been dispatched.
    FilterAndSendPacket(packet, Tail().state(), NOT_CONSUMED);
    return SUCCESS;
  }

  Tail().Push(packet);
  return SUCCESS;
}

void TouchDispositionGestureFilter::OnTouchEventAck(TouchEventAck ack_result) {
  // Spurious touch acks from the renderer should not trigger a crash.
  if (IsEmpty() || (Head().IsEmpty() && sequences_.size() == 1))
    return;

  if (Head().IsEmpty()) {
    CancelTapIfNecessary();
    CancelFlingIfNecessary();
    EndScrollIfNecessary();
    last_event_of_type_dropped_.clear();
    sequences_.pop();
  }

  GestureSequence& sequence = Head();

  // Dispatch the packet corresponding to the ack'ed touch, as well as any
  // additional timeout-based packets queued before the ack was received.
  bool touch_packet_for_current_ack_handled = false;
  while (!sequence.IsEmpty()) {
    const GestureEventDataPacket& packet = sequence.Front();
    DCHECK_NE(packet.gesture_source(), GestureEventDataPacket::UNDEFINED);
    DCHECK_NE(packet.gesture_source(), GestureEventDataPacket::INVALID);

    if (packet.gesture_source() != GestureEventDataPacket::TOUCH_TIMEOUT) {
      // We should handle at most one non-timeout based packet.
      if (touch_packet_for_current_ack_handled)
        break;
      sequence.UpdateState(packet.gesture_source(), ack_result);
      touch_packet_for_current_ack_handled = true;
    }
    FilterAndSendPacket(packet, sequence.state(), ack_result);
    sequence.Pop();
  }
  DCHECK(touch_packet_for_current_ack_handled);
}

bool TouchDispositionGestureFilter::IsEmpty() const {
  return sequences_.empty();
}

void TouchDispositionGestureFilter::FilterAndSendPacket(
    const GestureEventDataPacket& packet,
    const GestureSequence::GestureHandlingState& sequence_state,
    TouchEventAck ack_result) {
  for (size_t i = 0; i < packet.gesture_count(); ++i) {
    const GestureEventData& gesture = packet.gesture(i);
    DCHECK(ET_GESTURE_TYPE_FIRST <= gesture.type &&
           gesture.type <= ET_GESTURE_TYPE_LAST);
    if (IsGesturePrevented(gesture.type, ack_result, sequence_state)) {
      last_event_of_type_dropped_.mark_bit(GetGestureTypeIndex(gesture.type));
      CancelTapIfNecessary();
      continue;
    }
    last_event_of_type_dropped_.clear_bit(GetGestureTypeIndex(gesture.type));
    SendGesture(gesture);
  }
}

void TouchDispositionGestureFilter::SendGesture(const GestureEventData& event) {
  // TODO(jdduke): Factor out gesture stream reparation code into a standalone
  // utility class.
  switch (event.type) {
    case ET_GESTURE_LONG_TAP:
      CancelTapIfNecessary();
      CancelFlingIfNecessary();
      break;
    case ET_GESTURE_TAP_DOWN:
      DCHECK(!needs_tap_ending_event_);
      needs_tap_ending_event_ = true;
      break;
    case ET_GESTURE_TAP:
    case ET_GESTURE_TAP_CANCEL:
    case ET_GESTURE_TAP_UNCONFIRMED:
    case ET_GESTURE_DOUBLE_TAP:
      needs_tap_ending_event_ = false;
      break;
    case ET_GESTURE_SCROLL_BEGIN:
      CancelTapIfNecessary();
      CancelFlingIfNecessary();
      EndScrollIfNecessary();
      needs_scroll_ending_event_ = true;
      break;
    case ET_GESTURE_SCROLL_END:
      needs_scroll_ending_event_ = false;
      break;
    case ET_SCROLL_FLING_START:
      CancelFlingIfNecessary();
      needs_fling_ending_event_ = true;
      needs_scroll_ending_event_ = false;
      break;
    case ET_SCROLL_FLING_CANCEL:
      needs_fling_ending_event_ = false;
      break;
    default:
      break;
  }
  client_->ForwardGestureEvent(event);
}

void TouchDispositionGestureFilter::CancelTapIfNecessary() {
  if (!needs_tap_ending_event_)
    return;

  SendGesture(CreateGesture(ET_GESTURE_TAP_CANCEL));
  DCHECK(!needs_tap_ending_event_);
}

void TouchDispositionGestureFilter::CancelFlingIfNecessary() {
  if (!needs_fling_ending_event_)
    return;

  SendGesture(CreateGesture(ET_SCROLL_FLING_CANCEL));
  DCHECK(!needs_fling_ending_event_);
}

void TouchDispositionGestureFilter::EndScrollIfNecessary() {
  if (!needs_scroll_ending_event_)
    return;

  SendGesture(CreateGesture(ET_GESTURE_SCROLL_END));
  DCHECK(!needs_scroll_ending_event_);
}

// TouchDispositionGestureFilter::GestureSequence

TouchDispositionGestureFilter::GestureSequence::GestureHandlingState::
    GestureHandlingState()
    : seen_ack(false), start_consumed(false), no_consumer(false) {}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Head() {
  DCHECK(!sequences_.empty());
  return sequences_.front();
}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Tail() {
  DCHECK(!sequences_.empty());
  return sequences_.back();
}

TouchDispositionGestureFilter::GestureSequence::GestureSequence() {}

TouchDispositionGestureFilter::GestureSequence::~GestureSequence() {}

void TouchDispositionGestureFilter::GestureSequence::Push(
    const GestureEventDataPacket& packet) {
  packets_.push(packet);
}

void TouchDispositionGestureFilter::GestureSequence::Pop() {
  DCHECK(!IsEmpty());
  packets_.pop();
}

const GestureEventDataPacket&
TouchDispositionGestureFilter::GestureSequence::Front() const {
  DCHECK(!IsEmpty());
  return packets_.front();
}

void TouchDispositionGestureFilter::GestureSequence::UpdateState(
    GestureEventDataPacket::GestureSource gesture_source,
    TouchEventAck ack_result) {

  // Permanent states will not be affected by subsequent ack's.
  if (state_.no_consumer || state_.start_consumed)
    return;

  // |NO_CONSUMER| should only be effective when the *first* touch is ack'ed.
  if (!state_.seen_ack && ack_result == NO_CONSUMER_EXISTS) {
    state_.no_consumer = true;
  } else if (ack_result == CONSUMED) {
    if (gesture_source == GestureEventDataPacket::TOUCH_SEQUENCE_START ||
        gesture_source == GestureEventDataPacket::TOUCH_START) {
      state_.start_consumed = true;
    }
  }
  state_.seen_ack = true;
}

bool TouchDispositionGestureFilter::IsGesturePrevented(
    EventType gesture_type,
    TouchEventAck current,
    const GestureSequence::GestureHandlingState& state) const {

  if (state.no_consumer)
    return false;

  DispositionHandlingInfo disposition_handling_info =
      GetDispositionHandlingInfo(gesture_type);

  int required_touches = disposition_handling_info.required_touches;
  bool current_consumed = current == CONSUMED;
  if ((required_touches & RT_START && state.start_consumed) ||
      (required_touches & RT_CURRENT && current_consumed) ||
      (last_event_of_type_dropped_.has_bit(GetGestureTypeIndex(
          disposition_handling_info.antecedent_event_type)))) {
    return true;
  }
  return false;
}

bool TouchDispositionGestureFilter::GestureSequence::IsEmpty() const {
  return packets_.empty();
}

}  // namespace content
