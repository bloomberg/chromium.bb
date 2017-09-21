// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_MOJO_EVENT_STRUCT_TRAITS_H_
#define UI_EVENTS_MOJO_EVENT_STRUCT_TRAITS_H_

#include "ui/events/mojo/event.mojom.h"

namespace ui {
class Event;
class LatencyInfo;
}

namespace mojo {

using EventUniquePtr = std::unique_ptr<ui::Event>;

template <>
struct StructTraits<ui::mojom::EventDataView, EventUniquePtr> {
  static ui::mojom::EventType action(const EventUniquePtr& event);
  static int32_t flags(const EventUniquePtr& event);
  static base::TimeTicks time_stamp(const EventUniquePtr& event);
  static const ui::LatencyInfo& latency(const EventUniquePtr& event);
  static ui::mojom::KeyDataPtr key_data(const EventUniquePtr& event);
  static ui::mojom::PointerDataPtr pointer_data(const EventUniquePtr& event);
  static ui::mojom::GestureDataPtr gesture_data(const EventUniquePtr& event);
  static bool Read(ui::mojom::EventDataView r, EventUniquePtr* out);
};

}  // namespace mojo

#endif  // UI_EVENTS_MOJO_EVENT_STRUCT_TRAITS_H_
