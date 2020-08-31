// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_CHILD_STATUS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_CHILD_STATUS_COLLECTOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_report_interface.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_state_notifier.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"
#include "components/policy/proto/device_management_backend.pb.h"

class Profile;

namespace chromeos {
namespace system {
class StatisticsProvider;
}
}  // namespace chromeos

class PrefService;

namespace policy {

class ChildActivityStorage;
class ChildStatusCollectorState;

// Collects and summarizes the status of a child user in a Chrome OS device,
// also collecting some general (and limited) information about the device
// itself (e.g. OS version). Doesn't include anything related to other users on
// the device.
class ChildStatusCollector : public StatusCollector,
                             public chromeos::UsageTimeStateNotifier::Observer {
 public:
  // Passed into asynchronous mojo interface for communicating with Android.
  using AndroidStatusReceiver =
      base::Callback<void(const std::string&, const std::string&)>;

  // Calls the reporting mojo interface, passing over the AndroidStatusReceiver.
  // Returns false if the mojo interface isn't available, in which case no
  // asynchronous query is emitted and the android status query fails
  // synchronously. The |AndroidStatusReceiver| is not called in this case.
  using AndroidStatusFetcher =
      base::Callback<bool(const AndroidStatusReceiver&)>;

  // Constructor. Callers can inject their own *Fetcher callbacks, e.g. for unit
  // testing. A null callback can be passed for any *Fetcher parameter, to use
  // the default implementation. These callbacks are always executed on Blocking
  // Pool. Caller is responsible for passing already initialized |pref_service|.
  // |activity_day_start| indicates what time does the new day start for
  // activity reporting daily data aggregation. It is represented by the
  // distance from midnight.
  ChildStatusCollector(PrefService* pref_service,
                       Profile* profile,
                       chromeos::system::StatisticsProvider* provider,
                       const AndroidStatusFetcher& android_status_fetcher,
                       base::TimeDelta activity_day_start);
  ~ChildStatusCollector() override;

  // StatusCollector:
  void GetStatusAsync(const StatusCollectorCallback& response) override;
  void OnSubmittedSuccessfully() override;
  bool ShouldReportActivityTimes() const override;
  bool ShouldReportNetworkInterfaces() const override;
  bool ShouldReportUsers() const override;
  bool ShouldReportHardwareStatus() const override;
  bool ShouldReportCrashReportInfo() const override;

  // Returns the amount of time the child has used so far today. If there is no
  // user logged in, it returns 0.
  base::TimeDelta GetActiveChildScreenTime();

 protected:
  // chromeos::UsageTimeStateNotifier::Observer:
  void OnUsageTimeStateChange(
      chromeos::UsageTimeStateNotifier::UsageTimeState state) override;

  // Updates the child's active time.
  void UpdateChildUsageTime();

 private:
  // Callbacks from chromeos::VersionLoader.
  void OnOSVersion(const std::string& version);

  // Fetches all child data that is necessary to fill ChildStatusReportRequest.
  void FillChildStatusReportRequest(
      scoped_refptr<ChildStatusCollectorState> state);

  // Fetches user data related to the particular child user that is in the
  // session (i.e. it is not device data).
  bool FillUserSpecificFields(
      scoped_refptr<ChildStatusCollectorState> state,
      enterprise_management::ChildStatusReportRequest* status);

  // Helpers for the various portions of child status report. Return true if
  // they actually report any status. Functions that queue async queries take a
  // |ChildStatusCollectorState| instance.
  bool GetActivityTimes(
      enterprise_management::ChildStatusReportRequest* status);
  bool GetAppActivity(enterprise_management::ChildStatusReportRequest* status);
  bool GetVersionInfo(enterprise_management::ChildStatusReportRequest* status);
  // Queues async queries!
  bool GetAndroidStatus(const scoped_refptr<ChildStatusCollectorState>& state);

  // Update the cached values of the reporting settings.
  void UpdateReportingSettings();

  // Called to update the stored app activity data, after the report with app
  // activity was submitted.
  void OnAppActivityReportSubmitted();

  // Mainly used to store activity periods for reporting. Not owned.
  PrefService* const pref_service_;

  // Profile of the user that the status is collected for.
  Profile* const profile_;

  // The last time an active state check was performed.
  base::Time last_active_check_;

  // Whether the last state of the device was active. This is used for child
  // accounts only. Active is defined as having the screen turned on.
  bool last_state_active_ = true;

  // End timestamp of the latest activity that went into the last report
  // generated by GetStatusAsync(). Used to trim the stored data in
  // OnSubmittedSuccessfully(). Trimming is delayed so unsuccessful uploads
  // don't result in dropped data.
  int64_t last_reported_end_timestamp_ = 0;

  // The parameters associated with last app activity report.
  base::Optional<chromeos::app_time::AppActivityReportInterface::ReportParams>
      last_report_params_;

  base::RepeatingTimer update_child_usage_timer_;

  std::string os_version_;

  AndroidStatusFetcher android_status_fetcher_;

  // Stores and filters activity periods used for reporting.
  std::unique_ptr<ChildActivityStorage> activity_storage_;

  base::WeakPtrFactory<ChildStatusCollector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChildStatusCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_CHILD_STATUS_COLLECTOR_H_
