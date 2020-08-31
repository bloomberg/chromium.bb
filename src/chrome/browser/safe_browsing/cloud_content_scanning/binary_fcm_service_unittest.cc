// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

std::unique_ptr<KeyedService> BuildFakeGCMProfileService(
    content::BrowserContext* context) {
  return gcm::FakeGCMProfileService::Build(static_cast<Profile*>(context));
}

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID,
               instance_id::InstanceID*(const std::string& app_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class MockInstanceID : public instance_id::InstanceID {
 public:
  MockInstanceID() : InstanceID("", nullptr) {}
  ~MockInstanceID() override = default;

  MOCK_METHOD6(GetToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    base::TimeDelta time_to_live,
                    const std::map<std::string, std::string>& options,
                    std::set<Flags> flags,
                    GetTokenCallback callback));

  MOCK_METHOD3(DeleteToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    DeleteTokenCallback callback));

  void GetID(GetIDCallback callback) override { NOTIMPLEMENTED(); }
  void GetCreationTime(GetCreationTimeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

 protected:
  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteIDImpl(DeleteIDCallback callback) override { NOTIMPLEMENTED(); }
};

}  // namespace

class BinaryFCMServiceTest : public ::testing::Test {
 public:
  BinaryFCMServiceTest() {
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildFakeGCMProfileService));

    binary_fcm_service_ = BinaryFCMService::Create(&profile_);
  }

  Profile* profile() { return &profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BinaryFCMService> binary_fcm_service_;
};

TEST_F(BinaryFCMServiceTest, GetsInstanceID) {
  std::string received_instance_id = BinaryFCMService::kInvalidId;

  // Allow |binary_fcm_service_| to get an instance id.
  content::RunAllTasksUntilIdle();

  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));

  content::RunAllTasksUntilIdle();

  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);
}

TEST_F(BinaryFCMServiceTest, RoutesMessages) {
  DeepScanningClientResponse response1;
  DeepScanningClientResponse response2;

  binary_fcm_service_->SetCallbackForToken(
      "token1", base::BindRepeating(
                    [](DeepScanningClientResponse* target_response,
                       DeepScanningClientResponse response) {
                      *target_response = response;
                    },
                    &response1));
  binary_fcm_service_->SetCallbackForToken(
      "token2", base::BindRepeating(
                    [](DeepScanningClientResponse* target_response,
                       DeepScanningClientResponse response) {
                      *target_response = response;
                    },
                    &response2));

  DeepScanningClientResponse message;
  std::string serialized_message;
  gcm::IncomingMessage incoming_message;

  // Test that a message with token1 is routed only to the first callback.
  message.set_token("token1");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  base::Base64Encode(serialized_message, &serialized_message);
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.token(), "token1");
  EXPECT_EQ(response2.token(), "");

  // Test that a message with token2 is routed only to the second callback.
  message.set_token("token2");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  base::Base64Encode(serialized_message, &serialized_message);
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.token(), "token1");
  EXPECT_EQ(response2.token(), "token2");

  // Test that I can clear a callback
  response2.clear_token();
  binary_fcm_service_->ClearCallbackForToken("token2");
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response2.token(), "");
}

TEST_F(BinaryFCMServiceTest, EmitsHasKeyHistogram) {
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasKey", false, 1);
  }
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    incoming_message.data["proto"] = "proto";
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasKey", true, 1);
  }
}

TEST_F(BinaryFCMServiceTest, EmitsMessageParsedHistogram) {
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    incoming_message.data["proto"] = "invalid base 64";
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedBase64", false, 1);
  }
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    incoming_message.data["proto"] = "invalid+proto+data==";
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedBase64", true, 1);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedProto", false, 1);
  }
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;
    DeepScanningClientResponse message;
    std::string serialized_message;

    ASSERT_TRUE(message.SerializeToString(&serialized_message));
    base::Base64Encode(serialized_message, &serialized_message);
    incoming_message.data["proto"] = serialized_message;
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedBase64", true, 1);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedProto", true, 1);
  }
}

TEST_F(BinaryFCMServiceTest, EmitsMessageHasValidTokenHistogram) {
  gcm::IncomingMessage incoming_message;
  DeepScanningClientResponse message;

  message.set_token("token1");
  std::string serialized_message;
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  base::Base64Encode(serialized_message, &serialized_message);
  incoming_message.data["proto"] = serialized_message;

  {
    base::HistogramTester histograms;
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasValidToken", false, 1);
  }
  {
    binary_fcm_service_->SetCallbackForToken("token1", base::DoNothing());
    base::HistogramTester histograms;
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasValidToken", true, 1);
  }
}

TEST_F(BinaryFCMServiceTest, UnregisterToken) {
  // Get an instance ID
  std::string received_instance_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));
  content::RunAllTasksUntilIdle();
  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);

  // Delete it
  bool unregistered = false;
  binary_fcm_service_->UnregisterInstanceID(
      received_instance_id,
      base::BindOnce([](bool* target, bool value) { *target = value; },
                     &unregistered));
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(unregistered);
}

TEST_F(BinaryFCMServiceTest, UnregisterTokenRetriesFailures) {
  // Get an instance ID
  std::string received_instance_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));
  content::RunAllTasksUntilIdle();
  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);

  // Queue one failure, then success
  gcm::FakeGCMProfileService* gcm_service =
      static_cast<gcm::FakeGCMProfileService*>(
          gcm::GCMProfileServiceFactory::GetForProfile(&profile_));
  gcm_service->AddExpectedUnregisterResponse(gcm::GCMClient::NETWORK_ERROR);

  // Delete it
  bool unregistered = false;
  binary_fcm_service_->UnregisterInstanceID(
      received_instance_id,
      base::BindOnce([](bool* target, bool value) { *target = value; },
                     &unregistered));
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(unregistered);
}

TEST_F(BinaryFCMServiceTest, UnregistersTokensOnShutdown) {
  // Get an instance ID
  std::string received_instance_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));
  content::RunAllTasksUntilIdle();
  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);

  binary_fcm_service_->Shutdown();

  // Shutdown the InstanceID service.
  instance_id::InstanceIDProfileServiceFactory::GetInstance()
      ->SetTestingFactory(&profile_,
                          BrowserContextKeyedServiceFactory::TestingFactory());

  // Ensure we tear down correctly. This used to crash.
}

TEST_F(BinaryFCMServiceTest, UnregisterOneTokensOneCall) {
  MockInstanceIDDriver driver;
  MockInstanceID instance_id;
  ON_CALL(driver, GetInstanceID(_)).WillByDefault(Return(&instance_id));
  binary_fcm_service_.reset();
  binary_fcm_service_ = std::make_unique<BinaryFCMService>(
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      &driver);

  EXPECT_CALL(instance_id, GetToken(_, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    const std::map<std::string, std::string>&,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token",
                                    instance_id::InstanceID::Result::SUCCESS);
          }));

  std::string first_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &first_id));

  std::string second_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &second_id));

  content::RunAllTasksUntilIdle();

  EXPECT_CALL(instance_id, DeleteToken(_, _, _))
      .WillOnce(
          Invoke([](const std::string&, const std::string&,
                    instance_id::InstanceID::DeleteTokenCallback callback) {
            std::move(callback).Run(instance_id::InstanceID::Result::SUCCESS);
          }));

  binary_fcm_service_->UnregisterInstanceID(first_id, base::DoNothing());
  binary_fcm_service_->UnregisterInstanceID(second_id, base::DoNothing());

  content::RunAllTasksUntilIdle();
}

TEST_F(BinaryFCMServiceTest, UnregisterTwoTokensTwoCalls) {
  MockInstanceIDDriver driver;
  MockInstanceID instance_id;
  ON_CALL(driver, GetInstanceID(_)).WillByDefault(Return(&instance_id));
  binary_fcm_service_.reset();
  binary_fcm_service_ = std::make_unique<BinaryFCMService>(
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      &driver);

  EXPECT_CALL(instance_id, GetToken(_, _, _, _, _, _))
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    const std::map<std::string, std::string>&,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token",
                                    instance_id::InstanceID::Result::SUCCESS);
          }))
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    const std::map<std::string, std::string>&,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token 2",
                                    instance_id::InstanceID::Result::SUCCESS);
          }));

  std::string first_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &first_id));

  std::string second_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &second_id));

  content::RunAllTasksUntilIdle();

  EXPECT_CALL(instance_id, DeleteToken(_, _, _))
      .Times(2)
      .WillOnce(
          Invoke([](const std::string&, const std::string&,
                    instance_id::InstanceID::DeleteTokenCallback callback) {
            std::move(callback).Run(instance_id::InstanceID::Result::SUCCESS);
          }));

  binary_fcm_service_->UnregisterInstanceID(first_id, base::DoNothing());
  binary_fcm_service_->UnregisterInstanceID(second_id, base::DoNothing());

  content::RunAllTasksUntilIdle();
}

TEST_F(BinaryFCMServiceTest, QueuesGetInstanceIDOnRetriableError) {
  MockInstanceIDDriver driver;
  MockInstanceID instance_id;
  ON_CALL(driver, GetInstanceID(_)).WillByDefault(Return(&instance_id));
  binary_fcm_service_.reset();
  binary_fcm_service_ = std::make_unique<BinaryFCMService>(
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      &driver);

  EXPECT_CALL(instance_id, GetToken(_, _, _, _, _, _))
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    const std::map<std::string, std::string>&,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run(
                "", instance_id::InstanceID::Result::ASYNC_OPERATION_PENDING);
          }))
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    const std::map<std::string, std::string>&,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token",
                                    instance_id::InstanceID::Result::SUCCESS);
          }));

  std::string instance_id_token = BinaryFCMService::kInvalidId;
  binary_fcm_service_->SetQueuedOperationDelayForTesting(base::TimeDelta());
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &instance_id_token));

  content::RunAllTasksUntilIdle();

  EXPECT_NE(instance_id_token, BinaryFCMService::kInvalidId);
}

}  // namespace safe_browsing
