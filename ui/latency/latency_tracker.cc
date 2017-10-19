// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/latency/latency_histogram_macros.h"

namespace ui {
namespace {

std::string LatencySourceEventTypeToInputModalityString(
    ui::SourceEventType type) {
  switch (type) {
    case ui::SourceEventType::WHEEL:
      return "Wheel";
    case ui::SourceEventType::TOUCH:
      return "Touch";
    case ui::SourceEventType::KEY_PRESS:
      return "KeyPress";
    default:
      return "";
  }
}

}  // namespace

void LatencyTracker::OnGpuSwapBuffersCompleted(const LatencyInfo& latency) {
  LatencyInfo::LatencyComponent gpu_swap_end_component;
  if (!latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0,
          &gpu_swap_end_component)) {
    return;
  }

  LatencyInfo::LatencyComponent gpu_swap_begin_component;
  if (!latency.FindLatency(ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0,
                           &gpu_swap_begin_component)) {
    return;
  }

  LatencyInfo::LatencyComponent tab_switch_component;
  if (latency.FindLatency(ui::TAB_SHOW_COMPONENT, &tab_switch_component)) {
    base::TimeDelta delta =
        gpu_swap_end_component.event_time - tab_switch_component.event_time;
    for (size_t i = 0; i < tab_switch_component.event_count; i++) {
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration", delta);
    }
  }

  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           nullptr)) {
    return;
  }

  ui::SourceEventType source_event_type = latency.source_event_type();
  if (source_event_type == ui::SourceEventType::WHEEL ||
      source_event_type == ui::SourceEventType::TOUCH ||
      source_event_type == ui::SourceEventType::KEY_PRESS) {
    ComputeEndToEndLatencyHistograms(gpu_swap_begin_component,
                                     gpu_swap_end_component, latency);
  }

}

void LatencyTracker::ReportRapporScrollLatency(
    const std::string& name,
    const LatencyInfo::LatencyComponent& start_component,
    const LatencyInfo::LatencyComponent& end_component) {
  // TODO(mfomitchev): crbug.com/717629: Make RAPPOR or UKM reporting work with
  // Mus.
}

void LatencyTracker::ReportUkmScrollLatency(
    const std::string& event_name,
    const std::string& metric_name,
    const LatencyInfo::LatencyComponent& start_component,
    const LatencyInfo::LatencyComponent& end_component) {
  // TODO(mfomitchev): crbug.com/717629: Make RAPPOR or UKM reporting work with
  // Mus.
}

void LatencyTracker::ComputeEndToEndLatencyHistograms(
    const ui::LatencyInfo::LatencyComponent& gpu_swap_begin_component,
    const ui::LatencyInfo::LatencyComponent& gpu_swap_end_component,
    const ui::LatencyInfo& latency) {
  DCHECK(!latency.coalesced());
  if (latency.coalesced())
    return;

  LatencyInfo::LatencyComponent original_component;
  std::string scroll_name = "Uninitialized";

  const std::string input_modality =
      LatencySourceEventTypeToInputModalityString(latency.source_event_type());

  if (latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          &original_component)) {
    scroll_name = "ScrollBegin";
    // This UMA metric tracks the time between the final frame swap for the
    // first scroll event in a sequence and the original timestamp of that
    // scroll event's underlying touch/wheel event.
    UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(
        "Event.Latency.ScrollBegin." + input_modality +
            ".TimeToScrollUpdateSwapBegin2",
        original_component, gpu_swap_begin_component);

    ReportRapporScrollLatency("Event.Latency.ScrollBegin." + input_modality +
                                  ".TimeToScrollUpdateSwapBegin2",
                              original_component, gpu_swap_begin_component);

    ReportUkmScrollLatency("Event.ScrollBegin." + input_modality,
                           "TimeToScrollUpdateSwapBegin", original_component,
                           gpu_swap_begin_component);

    // TODO(lanwei): Will remove them when M56 is stable, see
    // https://crbug.com/669618.
    UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(
        "Event.Latency.ScrollUpdate." + input_modality +
            ".TimeToFirstScrollUpdateSwapBegin2",
        original_component, gpu_swap_begin_component);
  } else if (latency.FindLatency(
                 ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                 &original_component)) {
    scroll_name = "ScrollUpdate";
    // This UMA metric tracks the time from when the original touch event is
    // created to when the scroll gesture results in final frame swap.
    // First scroll events are excluded from this metric.
    if (input_modality == "Touch") {
      UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(
          "Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin2",
          original_component, gpu_swap_begin_component);

      ReportRapporScrollLatency(
          "Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin2",
          original_component, gpu_swap_begin_component);

      ReportUkmScrollLatency("Event.ScrollUpdate.Touch",
                             "TimeToScrollUpdateSwapBegin", original_component,
                             gpu_swap_begin_component);
    }
  } else if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                 &original_component)) {
    if (input_modality == "KeyPress") {
      UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(
          "Event.Latency.EndToEnd.KeyPress", original_component,
          gpu_swap_begin_component);
    }
    return;
  } else {
    // No original component found.
    return;
  }

  // Record scroll latency metrics.
  DCHECK(scroll_name == "ScrollBegin" || scroll_name == "ScrollUpdate");
  LatencyInfo::LatencyComponent rendering_scheduled_component;
  bool rendering_scheduled_on_main = latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT, 0,
      &rendering_scheduled_component);
  if (!rendering_scheduled_on_main) {
    if (!latency.FindLatency(
            ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, 0,
            &rendering_scheduled_component))
      return;
  }

  const std::string thread_name = rendering_scheduled_on_main ? "Main" : "Impl";

  UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(
      "Event.Latency." + scroll_name + "." + input_modality +
          ".TimeToHandled2_" + thread_name,
      original_component, rendering_scheduled_component);

  LatencyInfo::LatencyComponent renderer_swap_component;
  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, 0,
                           &renderer_swap_component))
    return;

  UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(
      "Event.Latency." + scroll_name + "." + input_modality +
          ".HandledToRendererSwap2_" + thread_name,
      rendering_scheduled_component, renderer_swap_component);

  LatencyInfo::LatencyComponent browser_received_swap_component;
  if (!latency.FindLatency(ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, 0,
                           &browser_received_swap_component))
    return;

  UMA_HISTOGRAM_SCROLL_LATENCY_SHORT_2(
      "Event.Latency." + scroll_name + "." + input_modality +
          ".RendererSwapToBrowserNotified2",
      renderer_swap_component, browser_received_swap_component);

  UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(
      "Event.Latency." + scroll_name + "." + input_modality +
          ".BrowserNotifiedToBeforeGpuSwap2",
      browser_received_swap_component, gpu_swap_begin_component);

  UMA_HISTOGRAM_SCROLL_LATENCY_SHORT_2(
      "Event.Latency." + scroll_name + "." + input_modality + ".GpuSwap2",
      gpu_swap_begin_component, gpu_swap_end_component);
}

}  // namespace ui
