// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include "base/barrier_closure.h"
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace enterprise_connectors {

using test::MockKeyRotationLauncher;
using RotateKeyCallback = DeviceTrustKeyManagerImpl::RotateKeyCallback;
using KeyRotationResult = DeviceTrustKeyManager::KeyRotationResult;

namespace {

constexpr char kFakeNonce[] = "fake nonce";
constexpr char kOtherFakeNonce[] = "other fake nonce";
constexpr char kFakeData[] = "some fake string";

constexpr char kLoadedKeyTrustLevelHistogram[] =
    "Enterprise.DeviceTrust.Key.TrustLevel";
constexpr char kLoadedKeyTypeHistogram[] = "Enterprise.DeviceTrust.Key.Type";
constexpr char kKeyCreationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.CreationResult";
constexpr char kKeyRotationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.RotationResult";

enterprise_connectors::test::MockKeyPersistenceDelegate::KeyInfo
CreateEmptyKey() {
  return {enterprise_management::BrowserPublicKeyUploadRequest::
              KEY_TRUST_LEVEL_UNSPECIFIED,
          std::vector<uint8_t>()};
}

}  // namespace

class DeviceTrustKeyManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    auto mock_launcher =
        std::make_unique<StrictMock<MockKeyRotationLauncher>>();
    mock_launcher_ = mock_launcher.get();

    key_manager_ =
        std::make_unique<DeviceTrustKeyManagerImpl>(std::move(mock_launcher));
  }

  void SetUpPersistedKey() {
    // ScopedKeyPersistenceDelegateFactory creates mocked persistence delegates
    // that already mimic the existence of a TPM key provider and stored key.
    auto mock_persistence_delegate =
        persistence_delegate_factory_.CreateMockedTpmDelegate();
    EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
    EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());

    persistence_delegate_factory_.set_next_instance(
        std::move(mock_persistence_delegate));
  }

  void SetUpNoKey() {
    auto mock_persistence_delegate =
        std::make_unique<test::MockKeyPersistenceDelegate>();
    EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
        .WillOnce(testing::Return(CreateEmptyKey()));

    persistence_delegate_factory_.set_next_instance(
        std::move(mock_persistence_delegate));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExpectLoadedTpmKeyMetrics(int times_loaded = 1) {
    // A TPM-generated key was successfully loaded. We don't know which
    // algorithm was used though, so just check that it was logged only once.
    histogram_tester_.ExpectUniqueSample(kLoadedKeyTrustLevelHistogram,
                                         DTKeyTrustLevel::kTpm, times_loaded);
    histogram_tester_.ExpectTotalCount(kLoadedKeyTypeHistogram, times_loaded);
  }

  void ExpectKeyCreatedMetrics() {
    histogram_tester_.ExpectUniqueSample(kKeyCreationResultHistogram,
                                         DTKeyRotationResult::kSucceeded, 1);
    histogram_tester_.ExpectTotalCount(kKeyRotationResultHistogram, 0);
  }

  void ExpectSuccessKeyRotateMetrics(int times_rotated = 1) {
    histogram_tester_.ExpectUniqueSample(kKeyRotationResultHistogram,
                                         DTKeyRotationResult::kSucceeded,
                                         times_rotated);
    histogram_tester_.ExpectTotalCount(kKeyCreationResultHistogram, 0);
  }

  void ExpectFailedKeyRotateMetrics() {
    histogram_tester_.ExpectUniqueSample(kKeyRotationResultHistogram,
                                         DTKeyRotationResult::kFailed, 1);
    histogram_tester_.ExpectTotalCount(kKeyCreationResultHistogram, 0);
  }

  void InitializeWithKey() {
    SetUpPersistedKey();

    key_manager_->StartInitialization();

    ExpectManagerHandlesRequests();

    ExpectLoadedTpmKeyMetrics();
    histogram_tester_.ExpectTotalCount(kKeyCreationResultHistogram, 0);
    histogram_tester_.ExpectTotalCount(kKeyRotationResultHistogram, 0);
  }

  // Expects that the key manager can handle an incoming request successfully,
  // which means it holds a valid key.
  void ExpectManagerHandlesRequests() {
    base::RunLoop run_loop;
    key_manager_->ExportPublicKeyAsync(base::BindLambdaForTesting(
        [&run_loop](absl::optional<std::string> value) {
          EXPECT_TRUE(value);
          EXPECT_FALSE(value->empty());
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  DeviceTrustKeyManagerImpl* key_manager() { return key_manager_.get(); }
  StrictMock<MockKeyRotationLauncher>* mock_launcher() {
    return mock_launcher_;
  }

  base::HistogramTester histogram_tester_;
  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<StrictMock<MockKeyRotationLauncher>> mock_launcher_;

  std::unique_ptr<DeviceTrustKeyManagerImpl> key_manager_;
};

// Tests that StartInitialization will load a key and not trigger key creation
// if key loading was successful.
TEST_F(DeviceTrustKeyManagerImplTest, Initialization_WithPersistedKey) {
  InitializeWithKey();
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful.
// - Key creation succeeds and a key gets loaded successfully,
// - Then a client request gets replied successfully.
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_CreatesKey_LoadKeySuccess) {
  SetUpNoKey();

  base::RunLoop create_key_loop;
  KeyRotationCommand::Callback key_rotation_callback;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            key_rotation_callback = std::move(callback);
            create_key_loop.Quit();
          }));

  key_manager()->StartInitialization();

  create_key_loop.Run();

  // Mimic that the key is now loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  // The manager should now respond to the callback as soon as the key is
  // loaded.
  base::RunLoop run_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&run_loop](absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  ExpectKeyCreatedMetrics();
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful.
// - Key creation succeeds, but the subsequent key loading fails.
// - Then a client request makes the key loading retry, but fail.
//   - But no key creation happens again
// - Then a second client request makes the key load successfully.
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_CreatesKey_LoadKeyFail_Retry) {
  SetUpNoKey();

  KeyRotationCommand::Callback key_rotation_callback;
  base::RunLoop create_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            key_rotation_callback = std::move(callback);
            create_key_loop.Quit();
          }));

  key_manager()->StartInitialization();

  create_key_loop.Run();

  // Mimic that the key creation was successful, however set key loading mocks
  // to mimic a loading failure.
  SetUpNoKey();
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  // The manager should now try to load the key upon receiving the client
  // request, but will fail to do so. It will then reply with a failure to the
  // client requests.
  base::RunLoop fail_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&fail_loop](absl::optional<std::string> value) {
        EXPECT_FALSE(value);
        fail_loop.Quit();
      }));

  fail_loop.Run();

  // Retry, but with a successful key loading this time.
  SetUpPersistedKey();

  base::RunLoop success_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&success_loop](absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        success_loop.Quit();
      }));

  success_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  ExpectKeyCreatedMetrics();
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful.
// - Key creation fails,
// - Subsequent client requests will retry key creation,
// - Key creation then succeeds,
// - Key loading succeeds,
// - The client request gets fulfilled.
TEST_F(DeviceTrustKeyManagerImplTest, Initialization_CreateFails_Retry) {
  SetUpNoKey();

  KeyRotationCommand::Callback failed_rotation_callback;
  base::RunLoop create_key_fail_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            failed_rotation_callback = std::move(callback);
            create_key_fail_loop.Quit();
          }));

  key_manager()->StartInitialization();

  create_key_fail_loop.Run();

  // Mimic that key creation failed.
  SetUpNoKey();
  ASSERT_FALSE(failed_rotation_callback.is_null());
  std::move(failed_rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(kKeyCreationResultHistogram,
                                       DTKeyRotationResult::kFailed, 1);

  KeyRotationCommand::Callback success_rotation_callback;
  base::RunLoop create_key_success_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            success_rotation_callback = std::move(callback);
            create_key_success_loop.Quit();
          }));

  // This client request will try to load the key, then fail (since key creation
  // failed previously), and then trigger a successful key creation followed
  // by a successful key loading.
  base::RunLoop request_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&request_loop](absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        request_loop.Quit();
      }));

  create_key_success_loop.Run();

  // Make the key creation return a successful status and fake that a key is
  // loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(success_rotation_callback.is_null());
  std::move(success_rotation_callback)
      .Run(KeyRotationCommand::Status::SUCCEEDED);

  // The client request should be responded to.
  request_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  histogram_tester_.ExpectTotalCount(kKeyRotationResultHistogram, 0);
  histogram_tester_.ExpectBucketCount(kKeyCreationResultHistogram,
                                      DTKeyRotationResult::kSucceeded, 1);
}

// Tests a long and specific chain of events which are, in sequence:
// - Key Manager initialization started,
// - Key loading starts,
// - Client requests (batch 1) come in and are pending,
// - Key loading fails,
// - Key creation process is launched,
// - New client requests (batch 2) come in and are pending,
// - Key creation process succeeds.
// - Key loading starts,
// - New client requests (batch 3) come in and are pending,
// - Key loading succeeds.
//   - All pending requests (batches 1, 2 and 3) are successfully answered.
// <end of test>
// This test also covers both client APIs (ExportPublicKeyAsync and
// SignStringAsync).
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_CreatesKey_SubsequentConcurrentCalls) {
  SetUpNoKey();

  KeyRotationCommand::Callback key_rotation_callback;
  base::RunLoop create_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            key_rotation_callback = std::move(callback);
            create_key_loop.Quit();
          }));

  key_manager()->StartInitialization();

  // A total of 6 callbacks will be marked as pending during this whole test.
  base::RunLoop barrier_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(6, barrier_loop.QuitClosure());

  auto export_key_counter = 0;
  auto export_key_callback =
      base::BindLambdaForTesting([&export_key_counter, &barrier_closure](
                                     absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        ++export_key_counter;
        barrier_closure.Run();
      });

  auto sign_string_counter = 0;
  auto sign_string_callback = base::BindLambdaForTesting(
      [&sign_string_counter,
       &barrier_closure](absl::optional<std::vector<uint8_t>> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        ++sign_string_counter;
        barrier_closure.Run();
      });

  // These initial requests should be queued-up since the key is currently being
  // created.
  key_manager()->ExportPublicKeyAsync(export_key_callback);
  key_manager()->SignStringAsync(kFakeData, sign_string_callback);

  create_key_loop.Run();

  // Key creation should not be triggered again.
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(_, _)).Times(0);

  // Queue-up more requests, which should also be set as pending since key
  // creation is still running.
  key_manager()->ExportPublicKeyAsync(export_key_callback);
  key_manager()->SignStringAsync(kFakeData, sign_string_callback);

  // Prepare for another key load, but with a valid key this time.
  SetUpPersistedKey();
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // Queue-up more requests, which should be executed normally.
  key_manager()->ExportPublicKeyAsync(export_key_callback);
  key_manager()->SignStringAsync(kFakeData, sign_string_callback);

  // All pending callbacks should get called now.
  barrier_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  ExpectKeyCreatedMetrics();
}

// Tests that a properly initialized key manager handles a successful rotate key
// request properly.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Simple_Success) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  absl::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));

  rotate_key_loop.Run();

  // Make the key rotation return a successful status and fake that a key is
  // loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // The key manager should now be properly setup.
  ExpectManagerHandlesRequests();
  ExpectSuccessKeyRotateMetrics();

  // The manager should have loaded a total of two keys.
  ExpectLoadedTpmKeyMetrics(/*times_loaded=*/2);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::SUCCESS);
}

// Tests that a properly initialized key manager handles a failing rotate key
// request properly.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Simple_Failed) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  absl::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));

  rotate_key_loop.Run();

  // Make the key rotation return a failed status.
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  // The key manager should still be properly setup.
  ExpectManagerHandlesRequests();
  ExpectFailedKeyRotateMetrics();

  // The manager should have loaded a total of one key, the initial one.
  ExpectLoadedTpmKeyMetrics(/*times_loaded=*/1);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::FAILURE);
}

// Tests that a properly initialized key manager handles concurrent successful
// rotate key request properly, which includes cancelling pending requests when
// another one is coming in.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Concurrent_Cancel_Success) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback first_rotation_callback;
  base::RunLoop first_rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            first_rotation_callback = std::move(callback);
            first_rotate_key_loop.Quit();
          }));

  // Create callback parameters for all calls.
  absl::optional<KeyRotationResult> first_captured_result;
  auto first_completion_callback = base::BindLambdaForTesting(
      [&first_captured_result](KeyRotationResult result) {
        first_captured_result = result;
      });
  absl::optional<KeyRotationResult> second_captured_result;
  auto second_completion_callback = base::BindLambdaForTesting(
      [&second_captured_result](KeyRotationResult result) {
        second_captured_result = result;
      });
  absl::optional<KeyRotationResult> third_captured_result;
  auto third_completion_callback = base::BindLambdaForTesting(
      [&third_captured_result](KeyRotationResult result) {
        third_captured_result = result;
      });

  // Kick off the concurrent rotation requests.
  key_manager()->RotateKey(kFakeNonce, std::move(first_completion_callback));
  key_manager()->RotateKey("irrelevant, random nonce.",
                           std::move(second_completion_callback));
  key_manager()->RotateKey(kOtherFakeNonce,
                           std::move(third_completion_callback));

  first_rotate_key_loop.Run();

  KeyRotationCommand::Callback second_rotation_callback;
  base::RunLoop second_rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kOtherFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            second_rotation_callback = std::move(callback);
            second_rotate_key_loop.Quit();
          }));

  // Make the key rotation return a successful status and fake that a
  // key again loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(first_rotation_callback.is_null());
  std::move(first_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  RunUntilIdle();

  second_rotate_key_loop.Run();

  // Make the second key rotation return a successful status and fake that a
  // key again loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(second_rotation_callback.is_null());
  std::move(second_rotation_callback)
      .Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // The key manager should still be properly setup.
  ExpectManagerHandlesRequests();
  ExpectSuccessKeyRotateMetrics(/*times_rotated=*/2);

  // The manager should have loaded a total of three keys.
  ExpectLoadedTpmKeyMetrics(/*times_loaded=*/3);

  ASSERT_TRUE(first_captured_result.has_value());
  ASSERT_TRUE(second_captured_result.has_value());
  ASSERT_TRUE(third_captured_result.has_value());
  ASSERT_EQ(first_captured_result.value(), KeyRotationResult::SUCCESS);
  ASSERT_EQ(second_captured_result.value(), KeyRotationResult::CANCELLATION);
  ASSERT_EQ(third_captured_result.value(), KeyRotationResult::SUCCESS);
}

// Tests that a properly initialized key manager handles concurrent rotate key
// request properly when the second one fails.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Concurrent_SuccessThenFail) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback first_rotation_callback;
  base::RunLoop first_rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            first_rotation_callback = std::move(callback);
            first_rotate_key_loop.Quit();
          }));

  // Create callback parameters for all calls.
  absl::optional<KeyRotationResult> first_captured_result;
  auto first_completion_callback = base::BindLambdaForTesting(
      [&first_captured_result](KeyRotationResult result) {
        first_captured_result = result;
      });
  absl::optional<KeyRotationResult> second_captured_result;
  auto second_completion_callback = base::BindLambdaForTesting(
      [&second_captured_result](KeyRotationResult result) {
        second_captured_result = result;
      });

  // Kick off the concurrent rotation requests.
  key_manager()->RotateKey(kFakeNonce, std::move(first_completion_callback));
  key_manager()->RotateKey(kOtherFakeNonce,
                           std::move(second_completion_callback));

  first_rotate_key_loop.Run();

  KeyRotationCommand::Callback second_rotation_callback;
  base::RunLoop second_rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kOtherFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            second_rotation_callback = std::move(callback);
            second_rotate_key_loop.Quit();
          }));

  // Make the key rotation return a successful status and fake that a key is
  // loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(first_rotation_callback.is_null());
  std::move(first_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  second_rotate_key_loop.Run();

  // Make the second key rotation return a failed status.
  ASSERT_FALSE(second_rotation_callback.is_null());
  std::move(second_rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  // The key manager should still be properly setup.
  ExpectManagerHandlesRequests();
  histogram_tester_.ExpectBucketCount(kKeyRotationResultHistogram,
                                      DTKeyRotationResult::kSucceeded, 1);
  histogram_tester_.ExpectBucketCount(kKeyRotationResultHistogram,
                                      DTKeyRotationResult::kFailed, 1);
  histogram_tester_.ExpectTotalCount(kKeyRotationResultHistogram, 2);
  histogram_tester_.ExpectTotalCount(kKeyCreationResultHistogram, 0);

  // The manager should have loaded a total of two keys.
  ExpectLoadedTpmKeyMetrics(/*times_loaded=*/2);

  ASSERT_TRUE(first_captured_result.has_value());
  ASSERT_TRUE(second_captured_result.has_value());
  ASSERT_EQ(first_captured_result.value(), KeyRotationResult::SUCCESS);
  ASSERT_EQ(second_captured_result.value(), KeyRotationResult::FAILURE);
}

// Tests that a properly initialized key manager handles a successful rotate key
// request properly when it receives it while already loading a key.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_AtLoadKey_Success) {
  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  // Binding the rotate request to the main thread, as the sequence checker will
  // be expecting that.
  absl::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  base::RepeatingClosure start_rotate = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), base::BindLambdaForTesting([&]() {
        key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));
      }));

  // Setup so that a key is loadable, but a rotate request is received at the
  // same time as it is being loaded.
  auto mock_persistence_delegate =
      persistence_delegate_factory_
          .CreateMockedTpmDelegateWithLoadingSideEffect(start_rotate);
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());

  persistence_delegate_factory_.set_next_instance(
      std::move(mock_persistence_delegate));

  // Starting initialization will start loading the key.
  key_manager()->StartInitialization();

  rotate_key_loop.Run();

  // Make the key rotation return a successful status and fake that a key is
  // loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // The key manager should now be properly setup.
  ExpectManagerHandlesRequests();
  ExpectSuccessKeyRotateMetrics();

  // The manager should have loaded a total of two keys.
  ExpectLoadedTpmKeyMetrics(/*times_loaded=*/2);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::SUCCESS);
}

// Tests that a properly initialized key manager handles a failed rotate key
// request properly when it receives it while already loading a key.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_AtLoadKey_Fails) {
  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  // Binding the rotate request to the main thread, as the sequence checker will
  // be expecting that.
  absl::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  base::RepeatingClosure start_rotate = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), base::BindLambdaForTesting([&]() {
        key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));
      }));

  // Setup so that a key is loadable, but a rotate request is received at the
  // same time as it is being loaded.
  auto mock_persistence_delegate =
      persistence_delegate_factory_
          .CreateMockedTpmDelegateWithLoadingSideEffect(start_rotate);
  EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());

  persistence_delegate_factory_.set_next_instance(
      std::move(mock_persistence_delegate));

  // Starting initialization will start loading the key.
  key_manager()->StartInitialization();

  rotate_key_loop.Run();

  // Make the key rotation return a failed status.
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  // The key manager should still be properly setup (using the old key).
  ExpectManagerHandlesRequests();
  ExpectFailedKeyRotateMetrics();

  // The manager should have loaded a total of one key.
  ExpectLoadedTpmKeyMetrics(/*times_loaded=*/1);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::FAILURE);
}

}  // namespace enterprise_connectors
