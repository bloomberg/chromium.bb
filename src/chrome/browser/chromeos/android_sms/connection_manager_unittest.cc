// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/android_sms/fake_connection_establisher.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "content/public/test/fake_service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace android_sms {

namespace {
const int64_t kDummyVersionId = 123l;
const int64_t kDummyVersionId2 = 456l;
}  // namespace

class ConnectionManagerTest : public testing::Test {
 protected:
  ConnectionManagerTest() = default;
  ~ConnectionManagerTest() override = default;

  void SetUp() override {
    fake_service_worker_context_ =
        std::make_unique<content::FakeServiceWorkerContext>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
  }

  void SetupConnectionManager(bool initial_feature_state) {
    SetFeatureState(initial_feature_state);
    std::unique_ptr<FakeConnectionEstablisher> fake_connection_establisher =
        std::make_unique<FakeConnectionEstablisher>();
    fake_connection_establisher_ = fake_connection_establisher.get();
    connection_manager_ = std::make_unique<ConnectionManager>(
        fake_service_worker_context_.get(),
        std::move(fake_connection_establisher),
        fake_multidevice_setup_client_.get());
  }

  void VerifyNumEstablishConnectionCalls(size_t count) {
    ASSERT_EQ(
        count,
        fake_connection_establisher_->establish_connection_calls().size());
    for (content::ServiceWorkerContext* service_worker_context :
         fake_connection_establisher_->establish_connection_calls())
      EXPECT_EQ(fake_service_worker_context_.get(), service_worker_context);
  }

  void SetFeatureState(bool enable) {
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kMessages,
        enable ? multidevice_setup::mojom::FeatureState::kEnabledByUser
               : multidevice_setup::mojom::FeatureState::kDisabledByUser);
  }

  content::FakeServiceWorkerContext* fake_service_worker_context() const {
    return fake_service_worker_context_.get();
  }

 private:
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  FakeConnectionEstablisher* fake_connection_establisher_;
  std::unique_ptr<content::FakeServiceWorkerContext>
      fake_service_worker_context_;
  std::unique_ptr<ConnectionManager> connection_manager_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionManagerTest);
};

TEST_F(ConnectionManagerTest, ConnectOnActivate) {
  SetupConnectionManager(true /* initial_feature_state */);
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that connection establishing is attempted on startup and
  // when a new version is activated.
  // startup + activate.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, ConnectOnNoControllees) {
  SetupConnectionManager(true /* initial_feature_state */);
  // Notify Activation so that Connection manager is tracking the version ID.
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that connection establishing is attempted when there are no
  // controllees.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate + no controllees.
  VerifyNumEstablishConnectionCalls(3u);
}

TEST_F(ConnectionManagerTest, IgnoreRedundantVersion) {
  SetupConnectionManager(true /* initial_feature_state */);
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Notify that current active version is now redundant.
  fake_service_worker_context()->NotifyObserversOnVersionRedundant(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that no connection establishing is attempted when there are no
  // controllees for the redundant version.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate only.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, ConnectOnNoControlleesWithNoActive) {
  SetupConnectionManager(true /* initial_feature_state */);
  // Verify that connection establishing is attempted when there are no
  // controllees for a version ID even if the activate notification was missed.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + no controllees.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, IgnoreOnNoControlleesInvalidId) {
  SetupConnectionManager(true /* initial_feature_state */);
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that OnNoControllees with different version id is ignored.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId2, GetAndroidMessagesURL());
  // startup + activate only.
  VerifyNumEstablishConnectionCalls(2u);
}

TEST_F(ConnectionManagerTest, InvalidScope) {
  SetupConnectionManager(true /* initial_feature_state */);
  GURL invalid_scope("https://example.com");
  // Verify that OnVersionActivated and OnNoControllees with invalid scope
  // are ignored
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, invalid_scope);
  fake_service_worker_context()->NotifyObserversOnNoControllees(kDummyVersionId,
                                                                invalid_scope);
  VerifyNumEstablishConnectionCalls(1u);  // startup only, ignore others.

  // Verify that OnVersionRedundant with invalid scope is ignored
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());
  fake_service_worker_context()->NotifyObserversOnVersionRedundant(
      kDummyVersionId, invalid_scope);
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate + no controllees.
  VerifyNumEstablishConnectionCalls(3u);
}

TEST_F(ConnectionManagerTest, FeatureStateInitDisabled) {
  // Verify that connection is not established on initialization
  // if the feature is not enabled.
  SetupConnectionManager(false /* initial_feature_state */);
  VerifyNumEstablishConnectionCalls(0u);

  SetFeatureState(true);
  VerifyNumEstablishConnectionCalls(1u);
}

TEST_F(ConnectionManagerTest, FeatureStateChange) {
  SetupConnectionManager(true /* initial_feature_state */);
  fake_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that disabling feature stops the service worker.
  SetFeatureState(false);
  VerifyNumEstablishConnectionCalls(2u);
  const auto& stop_calls = fake_service_worker_context()
                               ->stop_all_service_workers_for_origin_calls();
  ASSERT_EQ(1u, stop_calls.size());
  EXPECT_EQ(GetAndroidMessagesURL(), stop_calls[0]);

  // Verify that subsequent service worker events do not trigger connection.
  fake_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  VerifyNumEstablishConnectionCalls(2u);

  // Verify that enabling feature establishes connection again.
  SetFeatureState(true);
  VerifyNumEstablishConnectionCalls(3u);
}

}  // namespace android_sms

}  // namespace chromeos
