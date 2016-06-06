// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_EVENT_STRUCT_TRAITS_H_
#define COMPONENTS_MUS_PUBLIC_CPP_EVENT_STRUCT_TRAITS_H_

#include "components/mus/public/interfaces/input_events.mojom.h"

namespace ui {
class Event;
}

namespace mojo {

using EventUniquePtr = std::unique_ptr<ui::Event>;

template <>
struct StructTraits<mus::mojom::Event, EventUniquePtr> {
  static int32_t action(const EventUniquePtr& event);
  static int32_t flags(const EventUniquePtr& event);
  static int64_t time_stamp(const EventUniquePtr& event);
  static bool has_key_data(const EventUniquePtr& event);
  static mus::mojom::KeyDataPtr key_data(const EventUniquePtr& event);
  static bool has_pointer_data(const EventUniquePtr& event);
  static mus::mojom::PointerDataPtr pointer_data(const EventUniquePtr& event);
  static bool Read(mus::mojom::EventDataView r, EventUniquePtr* out);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_EVENT_STRUCT_TRAITS_H_
