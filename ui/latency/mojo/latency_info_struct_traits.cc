// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/mojo/latency_info_struct_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

namespace {

ui::mojom::SourceEventType UISourceEventTypeToMojo(ui::SourceEventType type) {
  switch (type) {
    case ui::UNKNOWN:
      return ui::mojom::SourceEventType::UNKNOWN;
    case ui::WHEEL:
      return ui::mojom::SourceEventType::WHEEL;
    case ui::MOUSE:
      return ui::mojom::SourceEventType::MOUSE;
    case ui::TOUCH:
      return ui::mojom::SourceEventType::TOUCH;
    case ui::INERTIAL:
      return ui::mojom::SourceEventType::INERTIAL;
    case ui::KEY_PRESS:
      return ui::mojom::SourceEventType::KEY_PRESS;
    case ui::TOUCHPAD:
      return ui::mojom::SourceEventType::TOUCHPAD;
    case ui::FRAME:
      return ui::mojom::SourceEventType::FRAME;
    case ui::OTHER:
      return ui::mojom::SourceEventType::OTHER;
  }
  NOTREACHED();
  return ui::mojom::SourceEventType::UNKNOWN;
}

ui::SourceEventType MojoSourceEventTypeToUI(ui::mojom::SourceEventType type) {
  switch (type) {
    case ui::mojom::SourceEventType::UNKNOWN:
      return ui::UNKNOWN;
    case ui::mojom::SourceEventType::WHEEL:
      return ui::WHEEL;
    case ui::mojom::SourceEventType::MOUSE:
      return ui::MOUSE;
    case ui::mojom::SourceEventType::TOUCH:
      return ui::TOUCH;
    case ui::mojom::SourceEventType::INERTIAL:
      return ui::INERTIAL;
    case ui::mojom::SourceEventType::KEY_PRESS:
      return ui::KEY_PRESS;
    case ui::mojom::SourceEventType::TOUCHPAD:
      return ui::TOUCHPAD;
    case ui::mojom::SourceEventType::FRAME:
      return ui::FRAME;
    case ui::mojom::SourceEventType::OTHER:
      return ui::OTHER;
  }
  NOTREACHED();
  return ui::SourceEventType::UNKNOWN;
}

}  // namespace

// static
const std::string&
StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::trace_name(
    const ui::LatencyInfo& info) {
  return info.trace_name_;
}

// static
const ui::LatencyInfo::LatencyMap&
StructTraits<ui::mojom::LatencyInfoDataView,
             ui::LatencyInfo>::latency_components(const ui::LatencyInfo& info) {
  return info.latency_components();
}

// static
int64_t StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::trace_id(
    const ui::LatencyInfo& info) {
  return info.trace_id();
}

// static
ukm::SourceId
StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::ukm_source_id(
    const ui::LatencyInfo& info) {
  return info.ukm_source_id();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::coalesced(
    const ui::LatencyInfo& info) {
  return info.coalesced();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::began(
    const ui::LatencyInfo& info) {
  return info.began();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::terminated(
    const ui::LatencyInfo& info) {
  return info.terminated();
}

// static
ui::mojom::SourceEventType
StructTraits<ui::mojom::LatencyInfoDataView,
             ui::LatencyInfo>::source_event_type(const ui::LatencyInfo& info) {
  return UISourceEventTypeToMojo(info.source_event_type());
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::Read(
    ui::mojom::LatencyInfoDataView data,
    ui::LatencyInfo* out) {
  if (!data.ReadTraceName(&out->trace_name_))
    return false;
  if (!data.ReadLatencyComponents(&out->latency_components_))
    return false;
  out->trace_id_ = data.trace_id();
  out->ukm_source_id_ = data.ukm_source_id();
  out->coalesced_ = data.coalesced();
  out->began_ = data.began();
  out->terminated_ = data.terminated();
  out->source_event_type_ = MojoSourceEventTypeToUI(data.source_event_type());

  return true;
}

// static
ui::mojom::LatencyComponentType
EnumTraits<ui::mojom::LatencyComponentType, ui::LatencyComponentType>::ToMojom(
    ui::LatencyComponentType type) {
  switch (type) {
    case ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
    case ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT;
    case ui::LATENCY_BEGIN_FRAME_RENDERER_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_FRAME_RENDERER_MAIN_COMPONENT;
    case ui::LATENCY_BEGIN_FRAME_RENDERER_INVALIDATE_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_FRAME_RENDERER_INVALIDATE_COMPONENT;
    case ui::LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT;
    case ui::LATENCY_BEGIN_FRAME_UI_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_FRAME_UI_MAIN_COMPONENT;
    case ui::LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT;
    case ui::LATENCY_BEGIN_FRAME_DISPLAY_COMPOSITOR_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_FRAME_DISPLAY_COMPOSITOR_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_UI_COMPONENT:
      return ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_UI_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
    case ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT:
      return ui::mojom::LatencyComponentType::
          DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT;
    case ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT;
  }
  NOTREACHED();
  return ui::mojom::LatencyComponentType::kMaxValue;
}

// static
bool EnumTraits<ui::mojom::LatencyComponentType, ui::LatencyComponentType>::
    FromMojom(ui::mojom::LatencyComponentType input,
              ui::LatencyComponentType* output) {
  switch (input) {
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT:
      *output = ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        LATENCY_BEGIN_FRAME_RENDERER_MAIN_COMPONENT:
      *output = ui::LATENCY_BEGIN_FRAME_RENDERER_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        LATENCY_BEGIN_FRAME_RENDERER_INVALIDATE_COMPONENT:
      *output = ui::LATENCY_BEGIN_FRAME_RENDERER_INVALIDATE_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT:
      *output = ui::LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::LATENCY_BEGIN_FRAME_UI_MAIN_COMPONENT:
      *output = ui::LATENCY_BEGIN_FRAME_UI_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT:
      *output = ui::LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        LATENCY_BEGIN_FRAME_DISPLAY_COMPOSITOR_COMPONENT:
      *output = ui::LATENCY_BEGIN_FRAME_DISPLAY_COMPOSITOR_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_UI_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_UI_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT:
      *output = ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT:
      *output = ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT;
      return true;
  }
  return false;
}

}  // namespace mojo
