// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace enterprise_reporting {

class RequestTimer;

// Schedules the next report and handles retry in case of error. It also cancels
// all pending uploads if the report policy is turned off.
class ReportScheduler {
 public:
  ReportScheduler(std::unique_ptr<policy::CloudPolicyClient> client,
                  std::unique_ptr<RequestTimer> request_timer);

  ~ReportScheduler();

 private:
  // Observes CloudReportingEnabled policy.
  void RegisterPerfObserver();

  // Handles kCloudReportingEnabled policy value change, including the first
  // policy value check during startup.
  void OnReportEnabledPerfChanged();

  // Schedules the first update request.
  void Start();

  // Generates a report and uploads it.
  void GenerateAndUploadReport();

  // Callback once report upload request is finished.
  void OnReportUploaded(bool status);

  // Policy value watcher
  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;

  std::unique_ptr<RequestTimer> request_timer_;

  DISALLOW_COPY_AND_ASSIGN(ReportScheduler);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_
