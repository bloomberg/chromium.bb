// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/pointer_events_handler.h"

#include <cmath>
#include <limits>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"

namespace fuchsia {
namespace ui {
namespace pointer {
// For using TouchInteractionId as a map key.
bool operator==(const TouchInteractionId& a, const TouchInteractionId& b) {
  return a.device_id == b.device_id && a.pointer_id == b.pointer_id &&
         a.interaction_id == b.interaction_id;
}
}  // namespace pointer
}  // namespace ui
}  // namespace fuchsia

namespace ui {

namespace fup = fuchsia::ui::pointer;

namespace {

void IssueTouchTraceEvent(const fup::TouchEvent& event) {
  DCHECK(event.has_trace_flow_id()) << "API guarantee";
  TRACE_EVENT_WITH_FLOW0("input", "dispatch_event_to_client",
                         event.trace_flow_id(), TRACE_EVENT_FLAG_FLOW_OUT);
}

void IssueMouseTraceEvent(const fup::MouseEvent& event) {
  DCHECK(event.has_trace_flow_id()) << "API guarantee";
  TRACE_EVENT_WITH_FLOW0("input", "dispatch_event_to_client",
                         event.trace_flow_id(), TRACE_EVENT_FLAG_FLOW_OUT);
}

bool HasValidTouchSample(const fup::TouchEvent& event) {
  if (!event.has_pointer_sample()) {
    return false;
  }
  DCHECK(event.pointer_sample().has_interaction()) << "API guarantee";
  DCHECK(event.pointer_sample().has_phase()) << "API guarantee";
  DCHECK(event.pointer_sample().has_position_in_viewport()) << "API guarantee";
  return true;
}

bool HasValidMouseSample(const fup::MouseEvent& event) {
  if (!event.has_pointer_sample()) {
    return false;
  }
  const auto& sample = event.pointer_sample();
  DCHECK(sample.has_device_id()) << "API guarantee";
  DCHECK(sample.has_position_in_viewport()) << "API guarantee";
  DCHECK(!sample.has_pressed_buttons() || sample.pressed_buttons().size() > 0)
      << "API guarantee";

  return true;
}

std::array<float, 2> ViewportToViewCoordinates(
    std::array<float, 2> viewport_coordinates,
    const std::array<float, 9>& viewport_to_view_transform) {
  // The transform matrix is a FIDL array with matrix data in column-major
  // order. For a matrix with data [a b c d e f g h i], and with the viewport
  // coordinates expressed as homogeneous coordinates, the logical view
  // coordinates are obtained with the following formula:
  //   |a d g|   |x|   |x'|
  //   |b e h| * |y| = |y'|
  //   |c f i|   |1|   |w'|
  // which we then normalize based on the w component:
  //   if w' not zero: (x'/w', y'/w')
  //   else (x', y')
  const auto& M = viewport_to_view_transform;
  const float x = viewport_coordinates[0];
  const float y = viewport_coordinates[1];
  const float xp = M[0] * x + M[3] * y + M[6];
  const float yp = M[1] * x + M[4] * y + M[7];
  const float wp = M[2] * x + M[5] * y + M[8];
  if (wp != 0) {
    return {xp / wp, yp / wp};
  } else {
    return {xp, yp};
  }
}

EventType GetEventTypeFromTouchEventPhase(fup::EventPhase phase) {
  switch (phase) {
    case fup::EventPhase::ADD:
      return ET_TOUCH_PRESSED;
    case fup::EventPhase::CHANGE:
      return ET_TOUCH_MOVED;
    case fup::EventPhase::REMOVE:
      return ET_TOUCH_RELEASED;
    case fup::EventPhase::CANCEL:
      return ET_TOUCH_CANCELLED;
  }
}

// TODO(crbug.com/1271730): Check if chrome gestures require strict boundaries.
std::array<float, 2> ClampToViewSpace(const float x,
                                      const float y,
                                      const fup::ViewParameters& p) {
  const float min_x = p.view.min[0];
  const float min_y = p.view.min[1];
  const float max_x = p.view.max[0];
  const float max_y = p.view.max[1];
  if (min_x <= x && x < max_x && min_y <= y && y < max_y) {
    return {x, y};  // No clamping to perform.
  }

  // View boundary is [min_x, max_x) x [min_y, max_y). Note that min is
  // inclusive, but max is exclusive - so we subtract epsilon.
  const float max_x_inclusive = std::nextafter(max_x, min_x);
  const float max_y_inclusive = std::nextafter(max_y, min_y);
  const float clamped_x = base::ranges::clamp(x, min_x, max_x_inclusive);
  const float clamped_y = base::ranges::clamp(y, min_y, max_y_inclusive);
  return {clamped_x, clamped_y};
}

EventType ComputeMouseEventType(bool any_button_down,
                                base::flat_set<uint32_t>& mouse_down,
                                uint32_t id) {
  bool mouse_is_down = mouse_down.count(id);
  if (!mouse_is_down && !any_button_down) {
    return ET_MOUSE_MOVED;
  } else if (!mouse_is_down && any_button_down) {
    mouse_down.insert(id);
    return ET_MOUSE_PRESSED;
  } else if (mouse_is_down && any_button_down) {
    return ET_MOUSE_DRAGGED;
  } else if (mouse_is_down && !any_button_down) {
    mouse_down.erase(id);
    return ET_MOUSE_RELEASED;
  }

  // TODO(fxbug.dev/88581): Compute ET_MOUSE_ENTERED and ET_MOUSE_EXITED types.

  NOTREACHED();
  return ET_MOUSE_RELEASED;
}

// It returns a "draft" because the coordinates are logical. FlatlandWindow
// might apply view pixel ratio to obtain physical coordinates.
//
// The gestures expect a gesture to start within the logical view space, and
// is not tolerant of floating point drift. This function coerces just the DOWN
// event's coordinate to start within the logical view.
TouchEvent CreateTouchEventDraft(const fup::TouchEvent& event,
                                 const fup::ViewParameters& view_parameters) {
  DCHECK(HasValidTouchSample(event)) << "precondition";
  const auto& sample = event.pointer_sample();
  const auto& interaction = sample.interaction();

  auto timestamp = base::TimeTicks::FromZxTime(event.timestamp());
  auto event_type = GetEventTypeFromTouchEventPhase(sample.phase());

  // TODO(crbug.com/1276571): Consider packing device_id field into PointerId.
  DCHECK_LE(interaction.pointer_id, 31U);
  PointerDetails pointer_details(EventPointerType::kTouch,
                                 interaction.pointer_id);
  // View parameters can change mid-interaction; apply transform on the fly.
  auto logical =
      ViewportToViewCoordinates(sample.position_in_viewport(),
                                view_parameters.viewport_to_view_transform);
  // TODO(fxbug.dev/88580): Consider setting hover via
  // ui::TouchEvent::set_hovering().
  return TouchEvent(event_type, gfx::PointF(logical[0], logical[1]),
                    gfx::PointF(sample.position_in_viewport()[0],
                                sample.position_in_viewport()[1]),
                    timestamp, pointer_details);
}

// It returns a "draft" because the coordinates are logical. Later,
// FlatlandWindow might apply view pixel ratio to obtain physical coordinates.
//
// Phase data is computed before this call; it involves state tracking based on
// button-down state.
//
// Button data, if available, gets packed into the |buttons_flags| field, in
// button order (kMousePrimaryButton, etc). The device-assigned button
// IDs are provided in priority order in MouseEvent.device_info (at the start of
// channel connection), and maps from device button ID (given in
// fup::MouseEvent) to Chrome ui::EventFlags.
//
// Scroll data, if available, gets packed into the |scroll_delta_x| or
// |scroll_delta_y| fields, and the |signal_kind| field is set to kScroll.
// The PointerDataPacketConverter reads this field to synthesize events to match
// Chrome's expected pointer stream.
//
// The gestures expect a gesture to start within the logical view space, and
// is not tolerant of floating point drift. This function coerces just the DOWN
// event's coordinate to start within the logical view.
MouseEvent CreateMouseEventDraft(const fup::MouseEvent& event,
                                 const EventType event_type,
                                 const fup::ViewParameters& view_parameters,
                                 const fup::MouseDeviceInfo& device_info) {
  DCHECK(HasValidMouseSample(event)) << "precondition";
  const auto& sample = event.pointer_sample();

  auto timestamp = base::TimeTicks::FromZxTime(event.timestamp());
  PointerDetails pointer_details(EventPointerType::kTouch, sample.device_id());
  // View parameters can change mid-interaction; apply transform on the fly.
  auto logical =
      ViewportToViewCoordinates(sample.position_in_viewport(),
                                view_parameters.viewport_to_view_transform);

  // Ensure gesture recognition: DOWN starts in the logical view space.
  if (event_type == ET_MOUSE_PRESSED) {
    logical = ClampToViewSpace(logical[0], logical[1], view_parameters);
  }

  int buttons_flags = 0;
  if (sample.has_pressed_buttons()) {
    const auto& pressed = sample.pressed_buttons();
    for (auto& button : pressed) {
      DCHECK(device_info.has_buttons()) << "API guarantee";
      // 0 maps to kPrimaryButton, and so on.
      if (button == device_info.buttons()[0]) {
        buttons_flags |= EF_LEFT_MOUSE_BUTTON;
      }
      if (button == device_info.buttons()[1]) {
        buttons_flags |= EF_RIGHT_MOUSE_BUTTON;
      }
      if (button == device_info.buttons()[2]) {
        buttons_flags |= EF_MIDDLE_MOUSE_BUTTON;
      }
    }
    DCHECK(buttons_flags != 0);
  }

  // TODO(fxbug.dev/88580): Use ui::MouseWheelEvent to signal scroll.

  return MouseEvent(event_type, gfx::PointF(logical[0], logical[1]),
                    gfx::PointF(sample.position_in_viewport()[0],
                                sample.position_in_viewport()[1]),
                    timestamp, buttons_flags, buttons_flags, pointer_details);
}

}  // namespace

PointerEventsHandler::PointerEventsHandler(fup::TouchSourceHandle touch_source,
                                           fup::MouseSourceHandle mouse_source)
    : touch_source_(touch_source.Bind()), mouse_source_(mouse_source.Bind()) {}

PointerEventsHandler::~PointerEventsHandler() = default;

// Core logic of this class.
// Aim to keep state management in this function.
void PointerEventsHandler::StartWatching(
    base::RepeatingCallback<void(Event*)> event_callback) {
  if (event_callback_) {
    LOG(ERROR) << "PointerEventsHandler::StartWatching() must be called once.";
    return;
  }
  event_callback_ = event_callback;

  // Start watching both channels.
  touch_source_->Watch(
      std::vector<fup::TouchResponse>(),
      fit::bind_member(this, &PointerEventsHandler::OnTouchSourceWatchResult));
  mouse_source_->Watch(
      fit::bind_member(this, &PointerEventsHandler::OnMouseSourceWatchResult));
}

// There are three basic interaction forms that we need to handle, and the API
// guarantees we see only these three forms. S=sample, R(g)=result-granted,
// R(d)=result-denied, and + means packaged in the same table. Time flows from
// left to right. Samples start with ADD, and end in REMOVE or CANCEL. Each
// interaction receives just one ownership result.
//   (1) Late grant. S S S R(g) S S S
//   (1-a) Combo.    S S S+R(g) S S S
//   (2) Early grant. S+R(g) S S S S S
//   (3) Late deny. S S S R(d)
//   (3-a) Combo.   S S S+R(d)
//
// This results in the following high-level algorithm to correctly deal with
// buffer allocation and deletion, and event flushing or event dropping based
// on ownership.
//   if event.sample.phase == ADD && !event.result
//     allocate buffer[event.sample.interaction]
//   if buffer[event.sample.interaction]
//     buffer[event.sample.interaction].push(event.sample)
//   else
//     flush_to_client(event.sample)
//   if event.result
//     if event.result == GRANTED
//       flush_to_client(buffer[event.result.interaction])
//     delete buffer[event.result.interaction]
void PointerEventsHandler::OnTouchSourceWatchResult(
    std::vector<fup::TouchEvent> events) {
  TRACE_EVENT0("input", "PointerEventsHandler::OnTouchSourceWatchResult");
  std::vector<fup::TouchResponse> touch_responses;
  for (const fup::TouchEvent& event : events) {
    IssueTouchTraceEvent(event);
    fup::TouchResponse
        response;  // Response per event, matched on event's index.
    if (event.has_view_parameters()) {
      touch_view_parameters_ = std::move(event.view_parameters());
    }
    if (HasValidTouchSample(event)) {
      const auto& sample = event.pointer_sample();
      const auto& interaction = sample.interaction();
      if (sample.phase() == fup::EventPhase::ADD &&
          !event.has_interaction_result()) {
        touch_buffer_.emplace(interaction, std::vector<TouchEvent>());
      }

      DCHECK(touch_view_parameters_.has_value()) << "API guarantee";
      auto draft = CreateTouchEventDraft(event, touch_view_parameters_.value());
      if (touch_buffer_.count(interaction) > 0) {
        touch_buffer_[interaction].emplace_back(std::move(draft));
      } else {
        event_callback_.Run(&draft);
      }

      // TODO(fxbug.dev/89296): Consider deriving response from
      // Event::handled().
      response.set_response_type(fup::TouchResponseType::YES);
    }
    if (event.has_interaction_result()) {
      const auto& result = event.interaction_result();
      const auto& interaction = result.interaction;
      if (result.status == fup::TouchInteractionStatus::GRANTED &&
          touch_buffer_.count(interaction) > 0) {
        for (auto& touch : touch_buffer_[interaction]) {
          event_callback_.Run(&touch);
        }
      }
      touch_buffer_.erase(interaction);  // Result seen, delete the buffer.
    }
    touch_responses.push_back(std::move(response));
  }

  touch_source_->Watch(
      std::move(touch_responses),
      fit::bind_member(this, &PointerEventsHandler::OnTouchSourceWatchResult));
}

void PointerEventsHandler::OnMouseSourceWatchResult(
    std::vector<fup::MouseEvent> events) {
  TRACE_EVENT0("input", "PointerEventsHandler::OnMouseSourceWatchResult");
  for (fup::MouseEvent& event : events) {
    IssueMouseTraceEvent(event);
    if (event.has_device_info()) {
      const auto& id = event.device_info().id();
      mouse_device_info_[id] = std::move(*event.mutable_device_info());
    }
    if (event.has_view_parameters()) {
      mouse_view_parameters_ = std::move(event.view_parameters());
    }
    if (HasValidMouseSample(event)) {
      const auto& sample = event.pointer_sample();
      const auto& id = sample.device_id();
      const bool any_button_down = sample.has_pressed_buttons();
      DCHECK(mouse_view_parameters_.has_value()) << "API guarantee";
      DCHECK(mouse_device_info_.count(id) > 0) << "API guarantee";

      const auto event_type =
          ComputeMouseEventType(any_button_down, mouse_down_, id);
      auto draft = CreateMouseEventDraft(event, event_type,
                                         mouse_view_parameters_.value(),
                                         mouse_device_info_[id]);
      event_callback_.Run(&draft);
    }
  }

  mouse_source_->Watch(
      fit::bind_member(this, &PointerEventsHandler::OnMouseSourceWatchResult));
}

}  // namespace ui
