// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_POINTER_EVENTS_HANDLER_H_
#define UI_EVENTS_FUCHSIA_POINTER_EVENTS_HANDLER_H_

#include <fuchsia/ui/pointer/cpp/fidl.h>

#include <array>
#include <functional>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"

namespace ui {

// Helper class for keying into a map.
struct InteractionLess {
  bool operator()(
      const fuchsia::ui::pointer::TouchInteractionId& interaction_id,
      const fuchsia::ui::pointer::TouchInteractionId& other_interaction_id)
      const {
    return (std::hash<uint32_t>()(interaction_id.device_id) ^
            std::hash<uint32_t>()(interaction_id.pointer_id) ^
            std::hash<uint32_t>()(interaction_id.interaction_id)) <
           (std::hash<uint32_t>()(other_interaction_id.device_id) ^
            std::hash<uint32_t>()(other_interaction_id.pointer_id) ^
            std::hash<uint32_t>()(other_interaction_id.interaction_id));
  }
};

// Channel processors for fuchsia.ui.pointer.TouchSource and MouseSource
// protocols. It manages the channel state, collects touch and mouse events, and
// surfaces them to FlatlandWindow as ui::Event events for further processing
// and dispatch. ui::Events have logical coordinates and they might be scaled by
// the view pixel ratio in FlatlandWindow to get physical coordinates.
class EVENTS_EXPORT PointerEventsHandler {
 public:
  PointerEventsHandler(fuchsia::ui::pointer::TouchSourceHandle touch_source,
                       fuchsia::ui::pointer::MouseSourceHandle mouse_source);
  ~PointerEventsHandler();

  // This function collects Fuchsia's TouchPointerSample and MousePointerSample
  // data and transforms them into ui::Events. It then calls the supplied
  // callback with a vector of ui::Events, which does last processing
  // (applies metrics).
  void StartWatching(base::RepeatingCallback<void(Event*)> event_callback);

 private:
  using MouseDeviceId = uint32_t;

  void OnTouchSourceWatchResult(
      std::vector<fuchsia::ui::pointer::TouchEvent> events);
  void OnMouseSourceWatchResult(
      std::vector<fuchsia::ui::pointer::MouseEvent> events);

  base::RepeatingCallback<void(Event*)> event_callback_;

  // Touch State ---------------------------------------------------------------
  // Channel for touch events from Scenic.
  fuchsia::ui::pointer::TouchSourcePtr touch_source_;

  // Receive touch events from Scenic. Must be copyable.
  fit::function<void(std::vector<fuchsia::ui::pointer::TouchEvent>)>
      touch_responder_;

  // Per-interaction buffer of touch events from Scenic. When an interaction
  // starts with event.pointer_sample.phase == ADD, we allocate a buffer and
  // store samples. When interaction ownership becomes
  // event.interaction_result.status == GRANTED, we flush the buffer to client,
  // delete the buffer, and all future events in this interaction are flushed
  // direct to client. When interaction ownership becomes DENIED, we delete the
  // buffer, and the client does not get any previous or future events in this
  // interaction.
  base::flat_map<fuchsia::ui::pointer::TouchInteractionId,
                 std::vector<TouchEvent>,
                 InteractionLess>
      touch_buffer_;

  // The fuchsia.ui.pointer.TouchSource protocol allows one in-flight
  // hanging-get Watch() call to gather touch events, and the client is expected
  // to respond with consumption intent on the following hanging-get Watch()
  // call. Store responses here for the next call.
  std::vector<fuchsia::ui::pointer::TouchResponse> touch_responses_;

  // The fuchsia.ui.pointer.TouchSource protocol issues channel-global view
  // parameters on connection and on change. Events must apply these view
  // parameters to correctly map to logical view coordinates. The "nullopt"
  // state represents the absence of view parameters, early in the protocol
  // lifecycle.
  absl::optional<fuchsia::ui::pointer::ViewParameters> touch_view_parameters_;

  // Mouse State ---------------------------------------------------------------
  // Channel for mouse events from Scenic.
  fuchsia::ui::pointer::MouseSourcePtr mouse_source_;

  // Receive mouse events from Scenic. Must be copyable.
  fit::function<void(std::vector<fuchsia::ui::pointer::MouseEvent>)>
      mouse_responder_;

  // The set of mouse devices that are currently interacting with the UI with
  // this high-level algorithm:
  //   if !mouse_down[id] && !button then: change = ET_MOUSE_MOVED
  //   if !mouse_down[id] &&  button then: change = ET_MOUSE_PRESSED;
  //       mouse_down.add(id)
  //   if  mouse_down[id] &&  button then: change = ET_MOUSE_DRAGGED
  //   if  mouse_down[id] && !button then: change = ET_MOUSE_RELEASED;
  //       mouse_down.remove(id)
  base::flat_set<MouseDeviceId> mouse_down_;

  // For each mouse device, its device-specific information, such as mouse
  // button priority order.
  base::flat_map<MouseDeviceId, fuchsia::ui::pointer::MouseDeviceInfo>
      mouse_device_info_;

  // The fuchsia.ui.pointer.MouseSource protocol issues channel-global view
  // parameters on connection and on change. Events must apply these view
  // parameters to correctly map to logical view coordinates. The "nullopt"
  // state represents the absence of view parameters, early in the protocol
  // lifecycle.
  absl::optional<fuchsia::ui::pointer::ViewParameters> mouse_view_parameters_;
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_POINTER_EVENTS_HANDLER_H_
