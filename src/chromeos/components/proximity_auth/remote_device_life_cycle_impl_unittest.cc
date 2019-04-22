// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/remote_device_life_cycle_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {

namespace {

// Subclass of RemoteDeviceLifeCycleImpl to make it testable.
class TestableRemoteDeviceLifeCycleImpl : public RemoteDeviceLifeCycleImpl {
 public:
  TestableRemoteDeviceLifeCycleImpl(
      chromeos::multidevice::RemoteDeviceRef remote_device,
      base::Optional<chromeos::multidevice::RemoteDeviceRef> local_device,
      chromeos::secure_channel::SecureChannelClient* secure_channel_client)
      : RemoteDeviceLifeCycleImpl(remote_device,
                                  local_device,
                                  secure_channel_client),
        remote_device_(remote_device) {}

  ~TestableRemoteDeviceLifeCycleImpl() override {}

 private:
  const chromeos::multidevice::RemoteDeviceRef remote_device_;

  DISALLOW_COPY_AND_ASSIGN(TestableRemoteDeviceLifeCycleImpl);
};

}  // namespace

class ProximityAuthRemoteDeviceLifeCycleImplTest
    : public testing::Test,
      public RemoteDeviceLifeCycle::Observer {
 protected:
  ProximityAuthRemoteDeviceLifeCycleImplTest()
      : test_remote_device_(
            chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        test_local_device_(
            chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        fake_secure_channel_client_(
            std::make_unique<
                chromeos::secure_channel::FakeSecureChannelClient>()),
        life_cycle_(test_remote_device_,
                    test_local_device_,
                    fake_secure_channel_client_.get()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  ~ProximityAuthRemoteDeviceLifeCycleImplTest() override {
    life_cycle_.RemoveObserver(this);
  }

  void CreateFakeConnectionAttempt() {
    auto fake_connection_attempt =
        std::make_unique<chromeos::secure_channel::FakeConnectionAttempt>();
    fake_connection_attempt_ = fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        test_remote_device_, test_local_device_,
        std::move(fake_connection_attempt));
  }

  void StartLifeCycle() {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::STOPPED, life_cycle_.GetState());
    life_cycle_.AddObserver(this);

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::STOPPED,
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
    life_cycle_.Start();
    task_runner_->RunUntilIdle();
    Mock::VerifyAndClearExpectations(this);

    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());
  }

  void SimulateSuccessfulAuthenticatedConnection() {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
                           RemoteDeviceLifeCycle::State::AUTHENTICATING));

    auto fake_client_channel =
        std::make_unique<chromeos::secure_channel::FakeClientChannel>();
    auto* fake_client_channel_raw = fake_client_channel.get();
    fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

    Mock::VerifyAndClearExpectations(this);

    EXPECT_CALL(*this,
                OnLifeCycleStateChanged(
                    RemoteDeviceLifeCycle::State::AUTHENTICATING,
                    RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
    task_runner_->RunUntilIdle();

    EXPECT_EQ(fake_client_channel_raw, life_cycle_.GetChannel());
    EXPECT_EQ(fake_client_channel_raw,
              life_cycle_.GetMessenger()->GetChannel());
  }

  void SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason
          failure_reason,
      RemoteDeviceLifeCycle::State expected_life_cycle_state) {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
                           expected_life_cycle_state));

    fake_connection_attempt_->NotifyConnectionAttemptFailure(failure_reason);

    EXPECT_EQ(nullptr, life_cycle_.GetChannel());
    EXPECT_EQ(nullptr, life_cycle_.GetMessenger());

    EXPECT_EQ(expected_life_cycle_state, life_cycle_.GetState());
  }

  MOCK_METHOD2(OnLifeCycleStateChanged,
               void(RemoteDeviceLifeCycle::State old_state,
                    RemoteDeviceLifeCycle::State new_state));

  chromeos::multidevice::RemoteDeviceRef test_remote_device_;
  chromeos::multidevice::RemoteDeviceRef test_local_device_;
  std::unique_ptr<chromeos::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  TestableRemoteDeviceLifeCycleImpl life_cycle_;

  chromeos::secure_channel::FakeConnectionAttempt* fake_connection_attempt_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthRemoteDeviceLifeCycleImplTest);
};

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Success) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateSuccessfulAuthenticatedConnection();
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR /* failure_reason */,
      RemoteDeviceLifeCycle::State::
          AUTHENTICATION_FAILED /* expected_life_cycle_state */);
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure_BluetoothNotPresent) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          ADAPTER_NOT_PRESENT /* failure_reason */,
      RemoteDeviceLifeCycle::State::STOPPED /* expected_life_cycle_state */);
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure_BluetoothNotPowered) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          ADAPTER_DISABLED /* failure_reason */,
      RemoteDeviceLifeCycle::State::STOPPED /* expected_life_cycle_state */);
}

}  // namespace proximity_auth
