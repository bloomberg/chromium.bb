// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_settings.h"

#include "base/trace_event/traced_value.h"

namespace cc {

SchedulerSettings::SchedulerSettings() = default;

SchedulerSettings::SchedulerSettings(const SchedulerSettings& other) = default;

SchedulerSettings::~SchedulerSettings() = default;

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
SchedulerSettings::AsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->SetBoolean("main_frame_while_submit_frame_throttled_enabled",
                    main_frame_while_submit_frame_throttled_enabled);
  state->SetBoolean("main_frame_before_activation_enabled",
                    main_frame_before_activation_enabled);
  state->SetBoolean("commit_to_active_tree", commit_to_active_tree);
  state->SetBoolean("timeout_and_draw_when_animation_checkerboards",
                    timeout_and_draw_when_animation_checkerboards);
  state->SetInteger("maximum_number_of_failed_draws_before_draw_is_forced",
                    maximum_number_of_failed_draws_before_draw_is_forced);
  state->SetBoolean("using_synchronous_renderer_compositor",
                    using_synchronous_renderer_compositor);
  state->SetInteger("background_frame_interval",
                    background_frame_interval.InMicroseconds());
  state->SetBoolean("enable_latency_recovery", enable_latency_recovery);
  state->SetBoolean("wait_for_all_pipeline_stages_before_draw",
                    wait_for_all_pipeline_stages_before_draw);
  state->SetBoolean("enable_surface_synchronization",
                    enable_surface_synchronization);
  return std::move(state);
}

}  // namespace cc
