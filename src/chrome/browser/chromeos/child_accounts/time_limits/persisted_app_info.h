// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_PERSISTED_APP_INFO_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_PERSISTED_APP_INFO_H_

#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

namespace base {
class Value;
}  // namespace base

namespace chromeos {
namespace app_time {

class PersistedAppInfo {
 public:
  static base::Optional<PersistedAppInfo> PersistedAppInfoFromDict(
      const base::Value* value,
      bool include_app_activity_array);
  static std::vector<PersistedAppInfo> PersistedAppInfosFromList(
      const base::Value* value,
      bool include_app_activity_array);
  static base::Optional<AppState> GetAppStateFromDict(const base::Value* value);

  PersistedAppInfo(const AppId& app_id,
                   AppState state,
                   base::TimeDelta active_running_time,
                   std::vector<AppActivity::ActiveTime> active_times);
  PersistedAppInfo(const PersistedAppInfo& info);
  PersistedAppInfo(PersistedAppInfo&& info);
  PersistedAppInfo& operator=(const PersistedAppInfo& info);
  PersistedAppInfo& operator=(PersistedAppInfo&& info);

  ~PersistedAppInfo();

  // Updates the dictionary to contain the information stored in this class.
  // If |replace_activity| is true, then completely replaces the list keyed by
  // |kActiveTimesKey| in the dicationary. Otherwise, appends the values in
  // |active_running_time_|.
  void UpdateAppActivityPreference(base::Value* dict,
                                   bool replace_activity) const;

  void RemoveActiveTimeEarlierThan(base::Time timestamp);

  bool ShouldRestoreApp() const;
  bool ShouldRemoveApp() const;

  const AppId& app_id() const { return app_id_; }
  AppState app_state() const { return app_state_; }
  base::TimeDelta active_running_time() const { return active_running_time_; }
  const std::vector<AppActivity::ActiveTime>& active_times() const {
    return active_times_;
  }

 private:
  AppId app_id_;
  AppState app_state_;
  base::TimeDelta active_running_time_;
  std::vector<AppActivity::ActiveTime> active_times_;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_PERSISTED_APP_INFO_H_
