// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

class CloudPolicyClient;
class DMAuth;

class POLICY_EXPORT RealtimeReportingJobConfiguration
    : public JobConfigurationBase {
 public:
  typedef base::OnceCallback<void(DeviceManagementService::Job* job,
                                  DeviceManagementStatus code,
                                  int net_error,
                                  const base::Value&)>
      Callback;

  RealtimeReportingJobConfiguration(CloudPolicyClient* client,
                                    std::unique_ptr<DMAuth> auth_data,
                                    Callback callback);

  ~RealtimeReportingJobConfiguration() override;

  // Add a new event to the payload.  This methods takes ownership of the event
  // value.  Events are dictionaries defined by the Event message described at
  // google/internal/chrome/reporting/v1/chromereporting.proto.
  void AddEvent(base::Value event);

 private:
  // Keys used in request payload dictionary.
  static const char kDmTokenKey[];
  static const char kClientIdKey[];
  static const char kEventsKey[];

  // DeviceManagementService::JobConfiguration.
  std::string GetPayload() override;
  std::string GetUmaName() override;
  void OnBeforeRetry() override {}
  void OnURLLoadComplete(DeviceManagementService::Job* job,
                         int net_error,
                         int response_code,
                         const std::string& response_body) override;

  // JobConfigurationBase overrides.
  GURL GetURL(int last_error) override;

  std::string server_url_;
  base::Value payload_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(RealtimeReportingJobConfiguration);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_REALTIME_REPORTING_JOB_CONFIGURATION_H_
