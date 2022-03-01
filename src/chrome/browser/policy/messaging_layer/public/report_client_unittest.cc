// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/messaging_layer/util/dm_token_retriever_provider.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/client/dm_token_retriever.h"
#include "components/reporting/client/mock_dm_token_retriever.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/encryption/decryption.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/testing_primitives.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace reporting {
namespace {

constexpr char kDMToken[] = "TOKEN";

class ReportClientTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Set up fake primary profile.
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<ash::FakeChromeUserManager>>();
    profile_ = std::make_unique<TestingProfile>(
        base::FilePath(FILE_PATH_LITERAL("/home/chronos/u-0123456789abcdef")));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), "12345"));
    const user_manager::User* user =
        mock_user_manager->AddPublicAccountUser(account_id);
    chromeos::ProfileHelper::Get()->SetActiveUserIdForTesting(
        profile_->GetProfileUserName());
    mock_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(mock_user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Encryption is enabled by default.
    ASSERT_TRUE(EncryptionModuleInterface::is_enabled());
    if (is_encryption_enabled()) {
      // Generate signing key pair.
      test::GenerateSigningKeyPair(signing_private_key_,
                                   signature_verification_public_key_);
      // Create decryption module.
      auto decryptor_result = test::Decryptor::Create();
      ASSERT_OK(decryptor_result.status()) << decryptor_result.status();
      decryptor_ = std::move(decryptor_result.ValueOrDie());
      // Prepare the key.
      signed_encryption_key_ = GenerateAndSignKey();
      // Disable connection to daemon.
      scoped_feature_list_.InitFromCommandLine("", "ConnectMissiveDaemon");
    } else {
      // Disable connection to daemon and encryption.
      scoped_feature_list_.InitFromCommandLine(
          "", "ConnectMissiveDaemon,EncryptedReporting");
    }

    // Provide a mock cloud policy client.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken(kDMToken);
    test_reporting_ = std::make_unique<ReportingClient::TestEnvironment>(
        base::FilePath(location_.GetPath()),
        base::StringPiece(
            reinterpret_cast<const char*>(signature_verification_public_key_),
            kKeySize),
        client_.get());

    // Use MockDMTokenRetriever and configure it to always return the test DM
    // token by default
    MockDMTokenRetrieverWithResult(kDMToken);
  }

  void TearDown() override {
    // Let everything ongoing to finish.
    task_environment_.RunUntilIdle();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.reset();
    profile_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  SignedEncryptionInfo GenerateAndSignKey() {
    DCHECK(decryptor_) << "Decryptor not created";
    // Generate new pair of private key and public value.
    uint8_t private_key[kKeySize];
    Encryptor::PublicKeyId public_key_id;
    uint8_t public_value[kKeySize];
    test::GenerateEncryptionKeyPair(private_key, public_value);
    test::TestEvent<StatusOr<Encryptor::PublicKeyId>> prepare_key_pair;
    decryptor_->RecordKeyPair(
        std::string(reinterpret_cast<const char*>(private_key), kKeySize),
        std::string(reinterpret_cast<const char*>(public_value), kKeySize),
        prepare_key_pair.cb());
    auto prepare_key_result = prepare_key_pair.result();
    DCHECK(prepare_key_result.ok());
    public_key_id = prepare_key_result.ValueOrDie();
    // Prepare public key to be delivered to Storage.
    SignedEncryptionInfo signed_encryption_key;
    signed_encryption_key.set_public_asymmetric_key(
        std::string(reinterpret_cast<const char*>(public_value), kKeySize));
    signed_encryption_key.set_public_key_id(public_key_id);
    // Sign public key.
    uint8_t value_to_sign[sizeof(Encryptor::PublicKeyId) + kKeySize];
    memcpy(value_to_sign, &public_key_id, sizeof(Encryptor::PublicKeyId));
    memcpy(value_to_sign + sizeof(Encryptor::PublicKeyId), public_value,
           kKeySize);
    uint8_t signature[kSignatureSize];
    test::SignMessage(
        signing_private_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature);
    signed_encryption_key.set_signature(
        std::string(reinterpret_cast<const char*>(signature), kSignatureSize));
    // Double check signature.
    DCHECK(VerifySignature(
        signature_verification_public_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature));
    return signed_encryption_key;
  }

  StatusOr<std::unique_ptr<ReportQueue>> CreateQueue() {
    auto config_result = ReportQueueConfiguration::Create(
        EventType::kUser, destination_, policy_checker_callback_);
    EXPECT_TRUE(config_result.ok()) << config_result.status();
    return CreateQueueWithConfig(std::move(config_result.ValueOrDie()));
  }

  StatusOr<std::unique_ptr<ReportQueue>> CreateQueueWithConfig(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config) {
    // Save a reference to report queue config so we can validate what DM token
    // was set later in the test
    report_queue_config_ = report_queue_config.get();
    test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> create_queue_event;
    ReportQueueProvider::CreateQueue(std::move(report_queue_config),
                                     create_queue_event.cb());
    auto report_queue_result = create_queue_event.result();

    // Let everything ongoing to finish.
    task_environment_.RunUntilIdle();

    return std::move(report_queue_result);
  }

  std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>
  CreateSpeculativeQueue() {
    auto config_result = ReportQueueConfiguration::Create(
        EventType::kUser, destination_, policy_checker_callback_);
    EXPECT_TRUE(config_result.ok()) << config_result.status();

    return CreateSpeculativeQueueWithConfig(
        std::move(config_result.ValueOrDie()));
  }

  std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>
  CreateSpeculativeQueueWithConfig(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config) {
    // Save a reference to report queue config so we can validate what DM token
    // was set
    report_queue_config_ = report_queue_config.get();
    auto speculative_queue_result = ReportQueueProvider::CreateSpeculativeQueue(
        std::move(report_queue_config));
    EXPECT_TRUE(speculative_queue_result.ok())
        << speculative_queue_result.status();
    return std::move(speculative_queue_result.ValueOrDie());
  }

  bool is_encryption_enabled() const { return GetParam(); }

  auto GetEncryptionKeyInvocation() {
    return [this](base::Value payload,
                  policy::CloudPolicyClient::ResponseCallback done_cb) {
      absl::optional<bool> const attach_encryption_settings =
          payload.FindBoolKey("attachEncryptionSettings");
      ASSERT_TRUE(attach_encryption_settings.has_value());
      ASSERT_TRUE(attach_encryption_settings.value());  // If set, must be true.
      ASSERT_TRUE(is_encryption_enabled());

      base::Value encryption_settings{base::Value::Type::DICTIONARY};
      std::string public_key;
      base::Base64Encode(signed_encryption_key_.public_asymmetric_key(),
                         &public_key);
      encryption_settings.SetStringKey("publicKey", public_key);
      encryption_settings.SetIntKey("publicKeyId",
                                    signed_encryption_key_.public_key_id());
      std::string public_key_signature;
      base::Base64Encode(signed_encryption_key_.signature(),
                         &public_key_signature);
      encryption_settings.SetStringKey("publicKeySignature",
                                       public_key_signature);
      base::Value response{base::Value::Type::DICTIONARY};
      response.SetPath("encryptionSettings", std::move(encryption_settings));
      std::move(done_cb).Run(std::move(response));
    };
  }

  auto GetVerifyDataInvocation() {
    return [this](base::Value payload,
                  policy::CloudPolicyClient::ResponseCallback done_cb) {
      base::Value* const records = payload.FindListKey("encryptedRecord");
      ASSERT_THAT(records, Ne(nullptr));
      base::Value::ListView records_list = records->GetList();
      ASSERT_THAT(records_list, SizeIs(1));
      base::Value& record = records_list[0];
      if (is_encryption_enabled()) {
        const base::Value* const enctyption_info =
            record.FindDictKey("encryptionInfo");
        ASSERT_THAT(enctyption_info, Ne(nullptr));
        const std::string* const encryption_key =
            enctyption_info->FindStringKey("encryptionKey");
        ASSERT_THAT(encryption_key, Ne(nullptr));
        const std::string* const public_key_id =
            enctyption_info->FindStringKey("publicKeyId");
        ASSERT_THAT(public_key_id, Ne(nullptr));
        int64_t key_id;
        ASSERT_TRUE(base::StringToInt64(*public_key_id, &key_id));
        EXPECT_THAT(key_id, Eq(signed_encryption_key_.public_key_id()));
      } else {
        ASSERT_THAT(record.FindKey("encryptionInfo"), Eq(nullptr));
      }
      base::Value* const seq_info = record.FindDictKey("sequenceInformation");
      ASSERT_THAT(seq_info, Ne(nullptr));
      base::Value response{base::Value::Type::DICTIONARY};
      response.SetPath("lastSucceedUploadedRecord", std::move(*seq_info));
      std::move(done_cb).Run(std::move(response));
    };
  }

  // Forces |DMTokenRetrieverProvider| to use the |MockDMTokenRetriever| by
  // default and return specified result through the completion callback on
  // trigger.
  void MockDMTokenRetrieverWithResult(
      const StatusOr<std::string> dm_token_result) {
    DMTokenRetrieverProvider::SetDMTokenRetrieverCallbackForTesting(
        base::BindRepeating(
            [](const StatusOr<std::string> dm_token_result,
               EventType event_type) -> std::unique_ptr<DMTokenRetriever> {
              auto dm_token_retriever =
                  std::make_unique<StrictMock<MockDMTokenRetriever>>();
              dm_token_retriever->ExpectRetrieveDMTokenAndReturnResult(
                  /*times=*/1, dm_token_result);
              return std::move(dm_token_retriever);
            },
            std::move(dm_token_result)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  // BrowserTaskEnvironment must be instantiated before other classes that posts
  // tasks.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ReportingClient::TestEnvironment> test_reporting_;

  base::ScopedTempDir location_;

  uint8_t signature_verification_public_key_[kKeySize];
  uint8_t signing_private_key_[kSignKeySize];
  scoped_refptr<test::Decryptor> decryptor_;
  SignedEncryptionInfo signed_encryption_key_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  raw_ptr<ReportQueueConfiguration> report_queue_config_;
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

// Tests that a ReportQueue can be created using the ReportingClient with a DM
// token.
//
// This scenario will eventually be deleted once we have migrated all events
// over to use event types instead.
TEST_P(ReportClientTest, CreatesReportQueueWithDMToken) {
  const base::StringPiece random_dm_token = "RANDOM DM TOKEN";
  auto report_queue_config_result = ReportQueueConfiguration::Create(
      random_dm_token, destination_, policy_checker_callback_);
  EXPECT_OK(report_queue_config_result);
  auto report_queue_result =
      CreateQueueWithConfig(std::move(report_queue_config_result.ValueOrDie()));
  ASSERT_OK(report_queue_result);
  ASSERT_THAT(std::move(report_queue_result.ValueOrDie()).get(), Ne(nullptr));
  EXPECT_THAT(report_queue_config_->dm_token(), StrEq(random_dm_token));
}

// Tests that a ReportQueue can be created using the ReportingClient given an
// event type.
TEST_P(ReportClientTest, CreatesReportQueueGivenEventType) {
  auto report_queue_result = CreateQueue();
  ASSERT_OK(report_queue_result);
  ASSERT_THAT(std::move(report_queue_result.ValueOrDie()).get(), Ne(nullptr));
  EXPECT_THAT(report_queue_config_->dm_token(), StrEq(kDMToken));
}

// Tests that a ReportQueue cannot be created when there is DM token retrieval
// failure
TEST_P(ReportClientTest, CreateReportQueueWhenDMTokenRetrievalFailure) {
  MockDMTokenRetrieverWithResult(
      Status(error::INTERNAL, "Simulated DM token retrieval failure"));
  auto report_queue_result = CreateQueue();
  EXPECT_FALSE(report_queue_result.ok());
  EXPECT_EQ(report_queue_result.status().error_code(), error::INTERNAL);
}

// Ensures that created ReportQueues are actually different.
TEST_P(ReportClientTest, CreatesTwoDifferentReportQueues) {
  // Create first queue.
  auto report_queue_result_1 = CreateQueue();
  ASSERT_OK(report_queue_result_1);

  // Create second queue. It will reuse the same ReportClient, so even if
  // encryption is enabled, there will be no roundtrip to server to get the key.
  auto report_queue_result_2 = CreateQueue();
  ASSERT_OK(report_queue_result_2);

  auto report_queue_1 = std::move(report_queue_result_1.ValueOrDie());
  auto report_queue_2 = std::move(report_queue_result_2.ValueOrDie());
  ASSERT_THAT(report_queue_1.get(), Ne(nullptr));
  ASSERT_THAT(report_queue_2.get(), Ne(nullptr));

  EXPECT_NE(report_queue_1.get(), report_queue_2.get());
}

// Creates queue, enqueues message and verifies it is uploaded.
TEST_P(ReportClientTest, EnqueueMessageAndUpload) {
  // Create queue.
  auto report_queue_result = CreateQueue();
  ASSERT_OK(report_queue_result);

  // Enqueue event.
  if (is_encryption_enabled()) {
    if (!StorageSelector::is_uploader_required() ||
        StorageSelector::is_use_missive()) {
      // Uploader is not available, cannot bring in the key.
      // Abort the test with no action.
      return;
    }

    // Uploader is available, let it set the key.
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArgs<0, 2>(Invoke(GetEncryptionKeyInvocation())))
        .RetiresOnSaturation();
  }

  test::TestEvent<Status> enqueue_record_event;
  std::move(report_queue_result.ValueOrDie())
      ->Enqueue("Record", FAST_BATCH, enqueue_record_event.cb());
  const auto enqueue_record_result = enqueue_record_event.result();
  EXPECT_OK(enqueue_record_result) << enqueue_record_result;

  if (StorageSelector::is_uploader_required() &&
      !StorageSelector::is_use_missive()) {
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArgs<0, 2>(Invoke(GetVerifyDataInvocation())));
  }

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

// Creates speculative queue, enqueues message and verifies it is uploaded
// eventually.
TEST_P(ReportClientTest, SpeculativelyEnqueueMessageAndUpload) {
  // Create queue.
  auto report_queue = CreateSpeculativeQueue();

  // Enqueue event.
  if (is_encryption_enabled()) {
    if (!StorageSelector::is_uploader_required() ||
        StorageSelector::is_use_missive()) {
      // Uploader is not available, cannot bring in the key.
      // Abort the test with no action.
      return;
    }
  }

  // Enqueue event right away, before attaching an actual queue.
  test::TestEvent<Status> enqueue_record_event;
  report_queue->Enqueue("Record", FAST_BATCH, enqueue_record_event.cb());
  const auto enqueue_record_result = enqueue_record_event.result();
  EXPECT_OK(enqueue_record_result) << enqueue_record_result;

  if (StorageSelector::is_uploader_required() &&
      !StorageSelector::is_use_missive()) {
    // Note: there does not seem to be another way to define the expectations
    // A+B for encrypted case and just B for non-encrypted.g
    if (is_encryption_enabled()) {
      EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
          .WillOnce(WithArgs<0, 2>(Invoke(GetEncryptionKeyInvocation())))
          .WillOnce(WithArgs<0, 2>(Invoke(GetVerifyDataInvocation())));
    } else {
      EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
          .WillOnce(WithArgs<0, 2>(Invoke(GetVerifyDataInvocation())));
    }
  }

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

INSTANTIATE_TEST_SUITE_P(ReportClientTestSuite,
                         ReportClientTest,
                         ::testing::Bool() /* true - encryption enabled */);

}  // namespace
}  // namespace reporting
