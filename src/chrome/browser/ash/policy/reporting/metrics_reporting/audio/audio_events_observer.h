// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_AUDIO_AUDIO_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_AUDIO_AUDIO_EVENTS_OBSERVER_H_

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_events_observer_base.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

class AudioEventsObserver
    : public reporting::CrosHealthdEventsObserverBase<
          chromeos::cros_healthd::mojom::CrosHealthdAudioObserver>,
      public chromeos::cros_healthd::mojom::CrosHealthdAudioObserver {
 public:
  AudioEventsObserver();

  AudioEventsObserver(const AudioEventsObserver& other) = delete;
  AudioEventsObserver& operator=(const AudioEventsObserver& other) = delete;

  ~AudioEventsObserver() override;

  // chromeos::cros_healthd::mojom::CrosHealthdAudioObserver:
  void OnUnderrun() override;

  void OnSevereUnderrun() override;

 protected:
  // CrosHealthdEventsObserverBase
  void AddObserver() override;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_AUDIO_AUDIO_EVENTS_OBSERVER_H_
