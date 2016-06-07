// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_MOJO_LATENCY_INFO_STRUCT_TRAITS_H_
#define UI_EVENTS_MOJO_LATENCY_INFO_STRUCT_TRAITS_H_

#include "ui/events/latency_info.h"
#include "ui/events/mojo/latency_info.mojom.h"

namespace mojo {

template <>
struct StructTraits<ui::mojom::InputCoordinate,
                    ui::LatencyInfo::InputCoordinate> {
  static float x(const ui::LatencyInfo::InputCoordinate& input) {
    return input.x;
  }
  static float y(const ui::LatencyInfo::InputCoordinate& input) {
    return input.y;
  }
  static bool Read(ui::mojom::InputCoordinateDataView data,
                   ui::LatencyInfo::InputCoordinate* out) {
    out->x = data.x();
    out->y = data.y();
    return true;
  }
};

// A buffer used to read bytes directly from LatencyInfoDataView into
// ui::LatencyInfo's input_coordinates_.
struct InputCoordinateArray {
  size_t size;
  ui::LatencyInfo::InputCoordinate* data;
};

// TODO(fsamuel): We should add a common ArrayTraits<CArray<T>> utility struct.
template <>
struct ArrayTraits<InputCoordinateArray> {
  using Element = ui::LatencyInfo::InputCoordinate;
  static size_t GetSize(const InputCoordinateArray& b);
  static Element* GetData(InputCoordinateArray& b);
  static const Element* GetData(const InputCoordinateArray& b);
  static Element& GetAt(InputCoordinateArray& b, size_t i);
  static const Element& GetAt(const InputCoordinateArray& b, size_t i);
  static void Resize(InputCoordinateArray& b, size_t size);
};

template <>
struct StructTraits<ui::mojom::LatencyComponent,
                    ui::LatencyInfo::LatencyComponent> {
  static int64_t sequence_number(
      const ui::LatencyInfo::LatencyComponent& component);
  static base::TimeTicks event_time(
      const ui::LatencyInfo::LatencyComponent& component);
  static uint32_t event_count(
      const ui::LatencyInfo::LatencyComponent& component);
  static bool Read(ui::mojom::LatencyComponentDataView data,
                   ui::LatencyInfo::LatencyComponent* out);
};

template <>
struct StructTraits<ui::mojom::LatencyComponentId,
                    std::pair<ui::LatencyComponentType, int64_t>> {
  static ui::mojom::LatencyComponentType type(
      const std::pair<ui::LatencyComponentType, int64_t>& id);
  static int64_t id(const std::pair<ui::LatencyComponentType, int64_t>& id);
  static bool Read(ui::mojom::LatencyComponentIdDataView data,
                   std::pair<ui::LatencyComponentType, int64_t>* out);
};

template <>
struct StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo> {
  static void* SetUpContext(const ui::LatencyInfo& info);
  static void TearDownContext(const ui::LatencyInfo& info, void* context);
  static const std::string& trace_name(const ui::LatencyInfo& info);
  static mojo::Array<ui::mojom::LatencyComponentPairPtr>& latency_components(
      const ui::LatencyInfo& info,
      void* context);
  static uint32_t input_coordinates_size(const ui::LatencyInfo& info);
  static InputCoordinateArray input_coordinates(const ui::LatencyInfo& info);
  static int64_t trace_id(const ui::LatencyInfo& info);
  static bool coalesced(const ui::LatencyInfo& info);
  static bool terminated(const ui::LatencyInfo& info);
  static bool Read(ui::mojom::LatencyInfoDataView data, ui::LatencyInfo* out);
};

}  // namespace mojo

#endif  // UI_EVENTS_MOJO_LATENCY_INFO_STRUCT_TRAITS_H_
