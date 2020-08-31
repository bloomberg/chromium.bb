// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SETTINGS_USER_ACTION_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SETTINGS_USER_ACTION_TRACKER_H_

#include "base/time/time.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/user_action_recorder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace settings {

// Records user actions which measure the effort required to change a setting.
// This class is only meant to track actions from an individual settings
// session; if the settings window is closed and reopened again, a new instance
// should be created for that new session.
class SettingsUserActionTracker : public mojom::UserActionRecorder {
 public:
  explicit SettingsUserActionTracker(
      mojo::PendingReceiver<mojom::UserActionRecorder> pending_receiver);
  SettingsUserActionTracker(const SettingsUserActionTracker& other) = delete;
  SettingsUserActionTracker& operator=(const SettingsUserActionTracker& other) =
      delete;
  ~SettingsUserActionTracker() override;

  // mojom::UserActionRecorder:
  void RecordPageFocus() override;
  void RecordPageBlur() override;
  void RecordClick() override;
  void RecordNavigation() override;
  void RecordSearch() override;
  void RecordSettingChange() override;

 private:
  friend class SettingsUserActionTrackerTest;

  // For unit tests.
  SettingsUserActionTracker();

  void ResetMetricsCountersAndTimestamp();

  // Time at which the last setting change metric was recorded since the window
  // has been focused, or null if no setting change has been recorded since the
  // window has been focused. Note that if the user blurs the window then
  // refocuses it in less than a minute, this value remains non-null; i.e., it
  // flips back to null only when the user has blurred the window for over a
  // minute.
  base::TimeTicks last_record_setting_changed_timestamp_;

  // Time at which recording the current metric has started. If
  // |has_changed_setting_| is true, we're currently measuring the "subsequent
  // setting change" metric; otherwise, we're measuring the "first setting
  // change" metric.
  base::TimeTicks metric_start_time_;

  // Counters associated with the current metric.
  size_t num_clicks_since_start_time_ = 0u;
  size_t num_navigations_since_start_time_ = 0u;
  size_t num_searches_since_start_time_ = 0u;

  // The last time at which a page blur event was received; if no blur events
  // have been received, this field is_null().
  base::TimeTicks last_blur_timestamp_;

  mojo::Receiver<mojom::UserActionRecorder> receiver_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SETTINGS_USER_ACTION_TRACKER_H_
