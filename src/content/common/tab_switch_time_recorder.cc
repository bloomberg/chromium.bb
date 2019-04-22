// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/tab_switch_time_recorder.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/presentation_feedback.h"

namespace content {

TabSwitchTimeRecorder::TabSwitchTimeRecorder() : weak_ptr_factory_(this) {}

TabSwitchTimeRecorder::~TabSwitchTimeRecorder() {}

base::OnceCallback<void(const gfx::PresentationFeedback&)>
TabSwitchTimeRecorder::BeginTimeRecording(
    const base::TimeTicks tab_switch_start_time,
    bool has_saved_frames,
    const base::TimeTicks render_widget_visibility_request_timestamp) {
  TRACE_EVENT_ASYNC_BEGIN0("latency", "TabSwitching::Latency",
                           TRACE_ID_LOCAL(this));

  // Reset all previously generated callbacks to enforce matching
  // BeginTimeRecording and DidPresentFrame calls. The reason to do this is
  // because sometimes, we could generate the callback on a tab switch, but no
  // frame submission occurs, potentially causing wrong metric to be uploaded on
  // the next tab switch. See crbug.com/936858 for more detail.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (tab_switch_start_time.is_null())
    return base::DoNothing();
  else
    return base::BindOnce(&TabSwitchTimeRecorder::DidPresentFrame, GetWeakPtr(),
                          has_saved_frames, tab_switch_start_time,
                          render_widget_visibility_request_timestamp);
}

void TabSwitchTimeRecorder::DidPresentFrame(
    bool has_saved_frames,
    base::TimeTicks tab_switch_start_time,
    base::TimeTicks render_widget_visibility_request_timestamp,
    const gfx::PresentationFeedback& feedback) {
  const auto delta = feedback.timestamp - tab_switch_start_time;

  if (has_saved_frames) {
    UMA_HISTOGRAM_TIMES("Browser.Tabs.TotalSwitchDuration.WithSavedFrames",
                        delta);
  } else {
    UMA_HISTOGRAM_TIMES("Browser.Tabs.TotalSwitchDuration.NoSavedFrames",
                        delta);
  }

  if (!render_widget_visibility_request_timestamp.is_null()) {
    TRACE_EVENT_ASYNC_END1("latency", "TabSwitching::Latency",
                           TRACE_ID_LOCAL(this), "latency",
                           delta.InMillisecondsF());
    UMA_HISTOGRAM_TIMES(
        "MPArch.RWH_TabSwitchPaintDuration",
        feedback.timestamp - render_widget_visibility_request_timestamp);
  }
}

}  // namespace content
