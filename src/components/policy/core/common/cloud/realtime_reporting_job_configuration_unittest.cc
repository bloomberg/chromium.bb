// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"

#include <set>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;
using testing::StrictMock;

namespace policy {

std::vector<std::string> ids = {"id1", "id2", "id3"};

constexpr char kAppPackage[] = "appPackage";
constexpr char kEventType[] = "eventType";
constexpr char kAppInstallEvent[] = "androidAppInstallEvent";
constexpr char kEventId[] = "eventId";
constexpr char kStatusCode[] = "status";

constexpr char kDummyToken[] = "dm_token";
constexpr char kPackage[] = "unitTestPackage";

const base::Value empty_response;

class MockCallbackObserver {
 public:
  MockCallbackObserver() = default;

  MOCK_METHOD4(OnURLLoadComplete,
               void(DeviceManagementService::Job* job,
                    DeviceManagementStatus code,
                    int net_error,
                    const base::Value&));
};

MATCHER_P(MatchDict, expected, "matches DictionaryValue") {
  return arg == expected;
}

class RealtimeReportingJobConfigurationTest : public testing::Test {
 public:
  RealtimeReportingJobConfigurationTest()
      : client_(&service_),
        configuration_(&client_,
                       DMAuth::FromDMToken(kDummyToken),
                       base::BindOnce(&MockCallbackObserver::OnURLLoadComplete,
                                      base::Unretained(&callback_observer_))) {}

  void SetUp() override {
    base::Value context(base::Value::Type::DICTIONARY);
    context.SetStringPath("browser.userAgent", "dummyAgent");
    base::Value events(base::Value::Type::LIST);
    for (size_t i = 0; i < ids.size(); ++i) {
      base::Value event = CreateEvent(ids[i], i);
      events.Append(std::move(event));
    }

    base::Value report = RealtimeReportingJobConfiguration::BuildReport(
        std::move(events), std::move(context));
    configuration_.AddReport(std::move(report));
  }

 protected:
  static base::Value CreateEvent(std::string& event_id, int type) {
    base::Value event(base::Value::Type::DICTIONARY);
    event.SetStringKey(kAppPackage, kPackage);
    event.SetIntKey(kEventType, type);
    base::Value wrapper(base::Value::Type::DICTIONARY);
    wrapper.SetKey(kAppInstallEvent, std::move(event));
    wrapper.SetStringKey(kEventId, event_id);
    return wrapper;
  }

  static base::Value CreateResponse(
      const std::set<std::string>& success_ids,
      const std::set<std::string>& failed_ids,
      const std::set<std::string>& permanent_failed_ids) {
    base::Value response(base::Value::Type::DICTIONARY);
    if (success_ids.size()) {
      base::Value* list =
          response.SetKey(RealtimeReportingJobConfiguration::kUploadedEventsKey,
                          base::Value(base::Value::Type::LIST));
      for (const auto& id : success_ids) {
        list->Append(id);
      }
    }
    if (failed_ids.size()) {
      base::Value* list =
          response.SetKey(RealtimeReportingJobConfiguration::kFailedUploadsKey,
                          base::Value(base::Value::Type::LIST));
      for (const auto& id : failed_ids) {
        base::Value failure(base::Value::Type::DICTIONARY);
        failure.SetStringKey(kEventId, id);
        failure.SetIntKey(kStatusCode, 8 /* RESOURCE_EXHAUSTED */);
        list->Append(std::move(failure));
      }
    }
    if (permanent_failed_ids.size()) {
      base::Value* list = response.SetKey(
          RealtimeReportingJobConfiguration::kPermanentFailedUploadsKey,
          base::Value(base::Value::Type::LIST));
      for (const auto& id : permanent_failed_ids) {
        base::Value failure(base::Value::Type::DICTIONARY);
        failure.SetStringKey(kEventId, id);
        failure.SetIntKey(kStatusCode, 9 /* FAILED_PRECONDITION */);
        list->Append(std::move(failure));
      }
    }

    return response;
  }

  static std::string CreateResponseString(const base::Value& response) {
    std::string response_string;
    base::JSONWriter::Write(response, &response_string);
    return response_string;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockDeviceManagementService service_;
  MockCloudPolicyClient client_;
  StrictMock<MockCallbackObserver> callback_observer_;
  DeviceManagementService::Job job_;
  RealtimeReportingJobConfiguration configuration_;
};

TEST_F(RealtimeReportingJobConfigurationTest, ValidatePayload) {
  base::Optional<base::Value> payload =
      base::JSONReader::Read(configuration_.GetPayload());
  EXPECT_TRUE(payload.has_value());
  EXPECT_EQ(kDummyToken, *payload->FindStringPath(
                             RealtimeReportingJobConfiguration::kDmTokenKey));
  EXPECT_EQ(client_.client_id(),
            *payload->FindStringPath(
                RealtimeReportingJobConfiguration::kClientIdKey));
  EXPECT_EQ(GetOSUsername(),
            *payload->FindStringPath(
                RealtimeReportingJobConfiguration::kMachineUserKey));
  EXPECT_EQ(version_info::GetVersionNumber(),
            *payload->FindStringPath(
                RealtimeReportingJobConfiguration::kChromeVersionKey));
  EXPECT_EQ(GetOSPlatform(),
            *payload->FindStringPath(
                RealtimeReportingJobConfiguration::kOsPlatformKey));
  EXPECT_EQ(GetOSVersion(),
            *payload->FindStringPath(
                RealtimeReportingJobConfiguration::kOsVersionKey));

  base::Value* events =
      payload->FindListKey(RealtimeReportingJobConfiguration::kEventsKey);
  EXPECT_EQ(ids.size(), events->GetList().size());
  int i = -1;
  for (const auto& event : events->GetList()) {
    auto* id = event.FindStringKey(kEventId);
    EXPECT_EQ(ids[++i], *id);
    auto type = event.FindKey(kAppInstallEvent)->FindIntKey(kEventType);
    EXPECT_EQ(i, *type);
  }
}

TEST_F(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_Success) {
  base::Value response = CreateResponse({ids[0], ids[1], ids[2]}, {}, {});
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_SUCCESS, net::OK,
                                testing::Eq(testing::ByRef(response))));
  configuration_.OnURLLoadComplete(&job_, net::OK,
                                   DeviceManagementService::kSuccess,
                                   CreateResponseString(response));
}

TEST_F(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_NetError) {
  int net_error = net::ERR_CONNECTION_RESET;
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_REQUEST_FAILED, net_error,
                                testing::Eq(testing::ByRef(empty_response))));
  configuration_.OnURLLoadComplete(&job_, net_error, 0, "");
}

TEST_F(RealtimeReportingJobConfigurationTest,
       OnURLLoadComplete_InvalidRequest) {
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_REQUEST_INVALID, net::OK,
                                testing::Eq(testing::ByRef(empty_response))));
  configuration_.OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kInvalidArgument, "");
}

TEST_F(RealtimeReportingJobConfigurationTest,
       OnURLLoadComplete_InvalidDMToken) {
  EXPECT_CALL(
      callback_observer_,
      OnURLLoadComplete(&job_, DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
                        net::OK, testing::Eq(testing::ByRef(empty_response))));
  configuration_.OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kInvalidAuthCookieOrDMToken, "");
}

TEST_F(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_NotSupported) {
  EXPECT_CALL(
      callback_observer_,
      OnURLLoadComplete(&job_, DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
                        net::OK, testing::Eq(testing::ByRef(empty_response))));
  configuration_.OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kDeviceManagementNotAllowed, "");
}

TEST_F(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_TempError) {
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_TEMPORARY_UNAVAILABLE, net::OK,
                                testing::Eq(testing::ByRef(empty_response))));
  configuration_.OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kServiceUnavailable, "");
}

TEST_F(RealtimeReportingJobConfigurationTest, OnURLLoadComplete_UnknownError) {
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_HTTP_STATUS_ERROR, net::OK,
                                testing::Eq(testing::ByRef(empty_response))));
  configuration_.OnURLLoadComplete(&job_, net::OK,
                                   DeviceManagementService::kInvalidURL, "");
}

TEST_F(RealtimeReportingJobConfigurationTest, ShouldRetry_Success) {
  auto response_string =
      CreateResponseString(CreateResponse({ids[0], ids[1], ids[2]}, {}, {}));
  auto should_retry = configuration_.ShouldRetry(
      DeviceManagementService::kSuccess, response_string);
  EXPECT_EQ(DeviceManagementService::Job::NO_RETRY, should_retry);
}

TEST_F(RealtimeReportingJobConfigurationTest, ShouldRetry_PartialFalure) {
  // Batch failures are retried
  auto response_string =
      CreateResponseString(CreateResponse({ids[0], ids[1]}, {ids[2]}, {}));
  auto should_retry = configuration_.ShouldRetry(
      DeviceManagementService::kSuccess, response_string);
  EXPECT_EQ(DeviceManagementService::Job::RETRY_WITH_DELAY, should_retry);
}

TEST_F(RealtimeReportingJobConfigurationTest, ShouldRetry_PermanentFailure) {
  // Permanent failures are not retried.
  auto response_string =
      CreateResponseString(CreateResponse({ids[0], ids[1]}, {}, {ids[2]}));
  auto should_retry = configuration_.ShouldRetry(
      DeviceManagementService::kSuccess, response_string);
  EXPECT_EQ(DeviceManagementService::Job::NO_RETRY, should_retry);
}

TEST_F(RealtimeReportingJobConfigurationTest, OnBeforeRetry_HttpFailure) {
  // No change should be made to the payload in this case.
  auto original_payload = configuration_.GetPayload();
  configuration_.OnBeforeRetry(DeviceManagementService::kServiceUnavailable,
                               "");
  EXPECT_EQ(original_payload, configuration_.GetPayload());
}

TEST_F(RealtimeReportingJobConfigurationTest, OnBeforeRetry_PartialBatch) {
  // Only those events whose ids are in failed_uploads should be in the payload
  // after the OnBeforeRetry call.
  auto response_string =
      CreateResponseString(CreateResponse({ids[0]}, {ids[1]}, {ids[2]}));
  configuration_.OnBeforeRetry(DeviceManagementService::kSuccess,
                               response_string);
  base::Optional<base::Value> payload =
      base::JSONReader::Read(configuration_.GetPayload());
  base::Value* events =
      payload->FindListKey(RealtimeReportingJobConfiguration::kEventsKey);
  EXPECT_EQ(1u, events->GetList().size());
  auto& event = events->GetList()[0];
  EXPECT_EQ(ids[1], *event.FindStringKey(kEventId));
}

}  // namespace policy
