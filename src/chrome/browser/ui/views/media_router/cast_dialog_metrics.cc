// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"

namespace media_router {

CastDialogMetrics::CastDialogMetrics(const base::Time& initialization_time)
    : initialization_time_(initialization_time) {}

CastDialogMetrics::~CastDialogMetrics() = default;

void CastDialogMetrics::OnSinksLoaded(const base::Time& sinks_load_time) {
  if (!sinks_load_time_.is_null())
    return;
  MediaRouterMetrics::RecordMediaRouterDialogLoaded(sinks_load_time -
                                                    initialization_time_);
  sinks_load_time_ = sinks_load_time;
}

void CastDialogMetrics::OnPaint(const base::Time& paint_time) {
  if (!paint_time_.is_null())
    return;
  MediaRouterMetrics::RecordMediaRouterDialogPaint(paint_time -
                                                   initialization_time_);
  paint_time_ = paint_time;
}

void CastDialogMetrics::OnStartCasting(const base::Time& start_time,
                                       int selected_sink_index) {
  DCHECK(!sinks_load_time_.is_null());
  MediaRouterMetrics::RecordStartRouteDeviceIndex(selected_sink_index);
  if (!first_action_recorded_) {
    MediaRouterMetrics::RecordStartLocalSessionLatency(start_time -
                                                       sinks_load_time_);
  }
  MaybeRecordFirstAction(MediaRouterUserAction::START_LOCAL);
}

void CastDialogMetrics::OnStopCasting() {
  // TODO(https://crbug.com/853369): In order to record this as the first user
  // action, we must determine whether we're stopping a local or non-local
  // route.
}

void CastDialogMetrics::OnCastModeSelected() {
  MaybeRecordFirstAction(MediaRouterUserAction::CHANGE_MODE);
}

void CastDialogMetrics::OnCloseDialog(const base::Time& close_time) {
  if (!first_action_recorded_ && !paint_time_.is_null())
    MediaRouterMetrics::RecordCloseDialogLatency(close_time - paint_time_);
  MaybeRecordFirstAction(MediaRouterUserAction::CLOSE);
}

void CastDialogMetrics::OnRecordSinkCount(int sink_count) {
  media_router::MediaRouterMetrics::RecordDeviceCount(sink_count);
}

void CastDialogMetrics::MaybeRecordFirstAction(MediaRouterUserAction action) {
  if (first_action_recorded_)
    return;
  // TODO(https://crbug.com/853369): Record initial user action once we can
  // determine whether a OnStopCasting() call is for a local or non-local route.
  first_action_recorded_ = true;
}

}  // namespace media_router
