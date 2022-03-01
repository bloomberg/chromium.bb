// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/audio/audio_events_observer.h"

#include <utility>

#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

AudioEventsObserver::AudioEventsObserver()
    : CrosHealthdEventsObserverBase<
          chromeos::cros_healthd::mojom::CrosHealthdAudioObserver>(this) {}

AudioEventsObserver::~AudioEventsObserver() = default;

void AudioEventsObserver::OnUnderrun() {
  // No action done with this type of event
}

void AudioEventsObserver::OnSevereUnderrun() {
  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(
      reporting::MetricEventType::AUDIO_SEVERE_UNDERRUN);
  OnEventObserved(std::move(metric_data));
}

void AudioEventsObserver::AddObserver() {
  chromeos::cros_healthd::ServiceConnection::GetInstance()->AddAudioObserver(
      BindNewPipeAndPassRemote());
}
}  // namespace reporting
