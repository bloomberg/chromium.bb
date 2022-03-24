// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/connection_manager_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace secure_channel {
namespace {
const char kSecureChannelFeatureName[] = "phone_hub";
const char kConnectionResultMetricName[] = "PhoneHub.Connection.Result";
const char kConnectionDurationMetricName[] = "PhoneHub.Connection.Duration";
const char kConnectionLatencyMetricName[] = "PhoneHub.Connectivity.Latency";

using multidevice_setup::mojom::HostStatus;

constexpr base::TimeDelta kFakeConnectionLatencyTime(base::Seconds(3u));
constexpr base::TimeDelta kFakeConnectionDurationTime(base::Seconds(10u));

constexpr base::TimeDelta kExpectedTimeoutSeconds(base::Seconds(15u));

class FakeObserver : public secure_channel::ConnectionManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t status_num_calls() const { return status_changed_num_calls_; }
  size_t message_cb_num_calls() const { return message_received_num_calls_; }

  const std::string& last_message() const { return last_message_; }

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override { ++status_changed_num_calls_; }
  void OnMessageReceived(const std::string& payload) override {
    last_message_ = payload;
    ++message_received_num_calls_;
  }

 private:
  size_t status_changed_num_calls_ = 0;
  size_t message_received_num_calls_ = 0;
  std::string last_message_;
};

}  // namespace

class ConnectionManagerImplTest : public testing::Test {
 protected:
  ConnectionManagerImplTest()
      : test_remote_device_(
            chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        test_local_device_(
            chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        fake_secure_channel_client_(
            std::make_unique<
                chromeos::secure_channel::FakeSecureChannelClient>()) {}

  ConnectionManagerImplTest(const ConnectionManagerImplTest&) = delete;
  ConnectionManagerImplTest& operator=(const ConnectionManagerImplTest&) =
      delete;
  ~ConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = timer.get();

    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(base::Time::UnixEpoch());

    fake_device_sync_client_.set_local_device_metadata(test_local_device_);
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(HostStatus::kHostVerified, test_remote_device_));
    connection_manager_ =
        base::WrapUnique(new secure_channel::ConnectionManagerImpl(
            &fake_multidevice_setup_client_, &fake_device_sync_client_,
            fake_secure_channel_client_.get(), std::move(timer),
            kSecureChannelFeatureName, kConnectionResultMetricName,
            kConnectionLatencyMetricName, kConnectionDurationMetricName,
            test_clock_.get()));
    connection_manager_->AddObserver(&fake_observer_);
    EXPECT_EQ(secure_channel::ConnectionManager::Status::kDisconnected,
              GetStatus());
  }

  void TearDown() override {
    connection_manager_->RemoveObserver(&fake_observer_);
  }

  secure_channel::ConnectionManager::Status GetStatus() const {
    return connection_manager_->GetStatus();
  }

  size_t GetNumStatusObserverCalls() const {
    return fake_observer_.status_num_calls();
  }

  size_t GetNumMessageReceivedObserverCalls() const {
    return fake_observer_.message_cb_num_calls();
  }

  void CreateFakeConnectionAttempt() {
    auto fake_connection_attempt =
        std::make_unique<chromeos::secure_channel::FakeConnectionAttempt>();
    fake_connection_attempt_ = fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_initiate_connection_attempt(
        test_remote_device_, test_local_device_,
        std::move(fake_connection_attempt));
  }

  void VerifyTimerSet() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    EXPECT_EQ(kExpectedTimeoutSeconds, mock_timer_->GetCurrentDelay());
  }

  void VerifyTimerStopped() { EXPECT_FALSE(mock_timer_->IsRunning()); }

  void InvokeTimerTask() {
    VerifyTimerSet();
    mock_timer_->Fire();
  }

  void VerifyConnectionResultHistogram(
      base::HistogramBase::Sample sample,
      base::HistogramBase::Count expected_count) {
    histogram_tester_.ExpectBucketCount(kConnectionResultMetricName, sample,
                                        expected_count);
  }

  base::MockOneShotTimer* mock_timer_;
  chromeos::multidevice::RemoteDeviceRef test_remote_device_;
  chromeos::multidevice::RemoteDeviceRef test_local_device_;
  device_sync::FakeDeviceSyncClient fake_device_sync_client_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  std::unique_ptr<chromeos::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<secure_channel::ConnectionManagerImpl> connection_manager_;
  FakeObserver fake_observer_;
  chromeos::secure_channel::FakeConnectionAttempt* fake_connection_attempt_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ConnectionManagerImplTest, SuccessfullyAttemptConnection) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptNearbyConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnecting,
            GetStatus());

  test_clock_->Advance(kFakeConnectionLatencyTime);

  auto fake_client_channel =
      std::make_unique<chromeos::secure_channel::FakeClientChannel>();
  fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Status has been updated to connected, verify that the status observer has
  // been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnected, GetStatus());

  histogram_tester_.ExpectTimeBucketCount(kConnectionLatencyMetricName,
                                          kFakeConnectionLatencyTime, 1);
  VerifyConnectionResultHistogram(true, 1);
}

TEST_F(ConnectionManagerImplTest, FailedToAttemptConnection) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptNearbyConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnecting,
            GetStatus());

  fake_connection_attempt_->NotifyConnectionAttemptFailure(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR);

  // Status has been updated to disconnected, verify that the status observer
  // has been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kDisconnected,
            GetStatus());

  VerifyConnectionResultHistogram(false, 1);
}

TEST_F(ConnectionManagerImplTest, SuccessfulAttemptConnectionButDisconnected) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptNearbyConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnecting,
            GetStatus());

  test_clock_->Advance(kFakeConnectionLatencyTime);

  auto fake_client_channel =
      std::make_unique<chromeos::secure_channel::FakeClientChannel>();
  chromeos::secure_channel::FakeClientChannel* fake_client_channel_raw =
      fake_client_channel.get();
  fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  // Status has been updated to connected, verify that the status observer has
  // been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnected, GetStatus());

  histogram_tester_.ExpectTimeBucketCount(kConnectionLatencyMetricName,
                                          kFakeConnectionLatencyTime, 1);
  VerifyConnectionResultHistogram(true, 1);

  // Simulate a disconnected channel.
  test_clock_->Advance(kFakeConnectionDurationTime);
  fake_client_channel_raw->NotifyDisconnected();

  // Expect status to be updated to disconnected.
  EXPECT_EQ(3u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kDisconnected,
            GetStatus());

  histogram_tester_.ExpectTimeBucketCount(kConnectionDurationMetricName,
                                          kFakeConnectionDurationTime, 1);
}

TEST_F(ConnectionManagerImplTest, AttemptConnectionWithMessageReceived) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptNearbyConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnecting,
            GetStatus());

  test_clock_->Advance(kFakeConnectionLatencyTime);

  auto fake_client_channel =
      std::make_unique<chromeos::secure_channel::FakeClientChannel>();
  chromeos::secure_channel::FakeClientChannel* fake_client_channel_raw =
      fake_client_channel.get();
  fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

  histogram_tester_.ExpectTimeBucketCount(kConnectionLatencyMetricName,
                                          kFakeConnectionLatencyTime, 1);
  VerifyConnectionResultHistogram(true, 1);

  // Status has been updated to connected, verify that the status observer has
  // been called.
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnected, GetStatus());

  // Simulate a message being sent.
  const std::string expected_payload = "payload";
  fake_client_channel_raw->NotifyMessageReceived(expected_payload);

  // Expected MessageReceived() callback to be called.
  EXPECT_EQ(1u, GetNumMessageReceivedObserverCalls());
  EXPECT_EQ(expected_payload, fake_observer_.last_message());
}

TEST_F(ConnectionManagerImplTest, AttemptConnectionWithoutLocalDevice) {
  // Simulate a missing local device.
  fake_device_sync_client_.set_local_device_metadata(
      absl::optional<chromeos::multidevice::RemoteDeviceRef>());
  connection_manager_->AttemptNearbyConnection();

  // Status is still disconnected since there is a missing device, verify that
  // the status observer did not get called (exited early).
  EXPECT_EQ(0u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kDisconnected,
            GetStatus());
}

TEST_F(ConnectionManagerImplTest, AttemptConnectionWithoutRemoteDevice) {
  // Simulate a missing remote device.
  fake_multidevice_setup_client_.SetHostStatusWithDevice(
      std::make_pair(HostStatus::kHostVerified,
                     absl::optional<chromeos::multidevice::RemoteDeviceRef>()));
  connection_manager_->AttemptNearbyConnection();

  // Status is still disconnected since there is a missing device, verify that
  // the status observer did not get called (exited early).
  EXPECT_EQ(0u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kDisconnected,
            GetStatus());
}

TEST_F(ConnectionManagerImplTest, ConnectionTimeout) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptNearbyConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnecting,
            GetStatus());
  VerifyTimerSet();

  // Simulate fast forwarding time to time out the connection request.
  InvokeTimerTask();

  VerifyTimerStopped();
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kDisconnected,
            GetStatus());
  VerifyConnectionResultHistogram(false, 1);
}

TEST_F(ConnectionManagerImplTest, DisconnectConnection) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptNearbyConnection();

  // Status has been updated to connecting, verify that the status observer
  // has been called.
  EXPECT_EQ(1u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kConnecting,
            GetStatus());
  VerifyTimerSet();

  // Disconnect the connection attempt.
  connection_manager_->Disconnect();
  EXPECT_EQ(2u, GetNumStatusObserverCalls());
  EXPECT_EQ(secure_channel::ConnectionManager::Status::kDisconnected,
            GetStatus());
  VerifyTimerStopped();
  VerifyConnectionResultHistogram(false, 1);
}

TEST_F(ConnectionManagerImplTest, RegisterPayloadFiles) {
  CreateFakeConnectionAttempt();
  connection_manager_->AttemptNearbyConnection();
  auto fake_client_channel =
      std::make_unique<chromeos::secure_channel::FakeClientChannel>();
  chromeos::secure_channel::FakeClientChannel* fake_client_channel_raw =
      fake_client_channel.get();
  fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));
  int registeration_result_count = 0;

  connection_manager_->RegisterPayloadFile(
      /*payload_id=*/1234, mojom::PayloadFiles::New(),
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        registeration_result_count++;
      }));
  connection_manager_->RegisterPayloadFile(
      /*payload_id=*/-5678, mojom::PayloadFiles::New(),
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        registeration_result_count++;
      }));

  EXPECT_EQ(2, registeration_result_count);
  EXPECT_EQ(2u, fake_client_channel_raw->registered_file_payloads().size());
  EXPECT_EQ(1234, fake_client_channel_raw->registered_file_payloads().at(0));
  EXPECT_EQ(-5678, fake_client_channel_raw->registered_file_payloads().at(1));
}

TEST_F(ConnectionManagerImplTest, RegisterPayloadFilesBeforeConnection) {
  int registeration_result_count = 0;

  connection_manager_->RegisterPayloadFile(
      /*payload_id=*/1234, mojom::PayloadFiles::New(),
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>(),
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        registeration_result_count++;
      }));

  EXPECT_EQ(1, registeration_result_count);
}

}  // namespace secure_channel
}  // namespace chromeos
