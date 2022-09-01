// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "connections/implementation/bwu_manager.h"

#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/fake_bwu_handler.h"
#include "connections/implementation/fake_endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/service_id_constants.h"
#include "internal/platform/exception.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr absl::string_view kServiceIdA = "ServiceA";
constexpr absl::string_view kServiceIdB = "ServiceB";
constexpr absl::string_view kEndpointId1 = "Endpoint1";
constexpr absl::string_view kEndpointId2 = "Endpoint2";
constexpr absl::string_view kEndpointId3 = "Endpoint3";
constexpr absl::string_view kEndpointId4 = "Endpoint4";

class BwuManagerTest : public ::testing::Test {
 protected:
  BwuManagerTest() {
    // Set up fake BWU handlers for WebRTC and WifiLAN.
    absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
    auto fake_web_rtc = std::make_unique<FakeBwuHandler>(Medium::WEB_RTC);
    auto fake_wifi_lan =
        std::make_unique<FakeBwuHandler>(Medium::WIFI_LAN);
    auto fake_wifi_hotspot =
        std::make_unique<FakeBwuHandler>(Medium::WIFI_HOTSPOT);
    fake_web_rtc_bwu_handler_ = fake_web_rtc.get();
    fake_wifi_lan_bwu_handler_ = fake_wifi_lan.get();
    fake_wifi_hotspot_bwu_handler_ = fake_wifi_hotspot.get();
    handlers.emplace(Medium::WEB_RTC, std::move(fake_web_rtc));
    handlers.emplace(Medium::WIFI_LAN, std::move(fake_wifi_lan));
    handlers.emplace(Medium::WIFI_HOTSPOT, std::move(fake_wifi_hotspot));

    BwuManager::Config config;
    config.allow_upgrade_to = BooleanMediumSelector{
        .web_rtc = true, .wifi_lan = true, .wifi_hotspot = true};

    bwu_manager_ = std::make_unique<BwuManager>(mediums_, em_, ecm_,
                                                std::move(handlers), config);

    // Don't run tasks on other threads. Avoids race conditions in tests.
    bwu_manager_->MakeSingleThreadedForTesting();
  }

  ~BwuManagerTest() override { bwu_manager_->Shutdown(); }

  // Create the initial device-to-device connection, before bandwidth upgrade.
  // Typically |medium| will be Bluetooth.
  FakeEndpointChannel* CreateInitialEndpoint(absl::string_view service_id,
                                             absl::string_view endpoint_id,
                                             Medium medium) {
    auto channel =
        std::make_unique<FakeEndpointChannel>(medium, std::string(service_id));
    FakeEndpointChannel* channel_raw = channel.get();
    ecm_.RegisterChannelForEndpoint(&client_, std::string(endpoint_id),
                                    std::move(channel));
    return channel_raw;
  }

  // Upgrade from |initial_medium| to |upgrade_medium|, close down the BLUETOOTH
  // channel, return the upgraded endpoint channel. This logic is tested in
  // InitiateBwu_Success; we use this function in subsequent tests for
  // convenience.
  FakeEndpointChannel* FullyUpgradeEndpoint(absl::string_view endpoint_id,
                                            Medium initial_medium,
                                            Medium upgrade_medium) {
    FakeBwuHandler* handler = nullptr;
    switch (upgrade_medium) {
      case Medium::WEB_RTC:
        handler = fake_web_rtc_bwu_handler_;
        break;
      case Medium::WIFI_LAN:
        handler = fake_wifi_lan_bwu_handler_;
        break;
      case Medium::WIFI_HOTSPOT:
        handler = fake_wifi_hotspot_bwu_handler_;
        break;
      default:
        return nullptr;
    }

    // Create upgraded channel.
    bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(endpoint_id),
                                         upgrade_medium);
    FakeEndpointChannel* upgraded_channel =
        handler->NotifyBwuManagerOfIncomingConnection(
            handler->handle_initialize_calls().size() - 1, bwu_manager_.get());

    // Close initial channel.
    ExceptionOr<OfflineFrame> last_write_frame =
        parser::FromBytes(parser::ForBwuLastWrite());
    bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                  std::string(endpoint_id), &client_,
                                  initial_medium);
    ExceptionOr<OfflineFrame> safe_to_close_frame =
        parser::FromBytes(parser::ForBwuSafeToClose());
    bwu_manager_->OnIncomingFrame(safe_to_close_frame.result(),
                                  std::string(endpoint_id), &client_,
                                  initial_medium);

    return upgraded_channel;
  }

  ClientProxy client_;
  EndpointChannelManager ecm_;
  EndpointManager em_{&ecm_};
  // It's okay there are no actual Mediums (i.e., implementations). These won't
  // be needed if we pass in an explict medium to InitiateBwuForEndpoint.
  Mediums mediums_;
  FakeBwuHandler* fake_web_rtc_bwu_handler_ = nullptr;
  FakeBwuHandler* fake_wifi_lan_bwu_handler_ = nullptr;
  FakeBwuHandler* fake_wifi_hotspot_bwu_handler_ = nullptr;
  std::unique_ptr<BwuManager> bwu_manager_;
};

class BwuManagerTestParam : public BwuManagerTest,
                            public ::testing::WithParamInterface<bool> {
 protected:
  BwuManagerTestParam() {
    FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums =
        GetParam();
  }
};

TEST_P(BwuManagerTestParam, InitiateBwu_Success) {
  // Create the initial device-to-device Bluetooth connection.
  FakeEndpointChannel* initial_channel =
      CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  // Initiate BWU, and send BANDWIDTH_UPGRADE_NEGOTIATION.UPGRADE_PATH_AVAILABLE
  // to the Responder over the initial Bluetooth channel.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);

  // The appropriate upgrade medium handler is informed of the BWU initiation.
  ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_EQ(WrapInitiatorUpgradeServiceId(kServiceIdA),
            fake_web_rtc_bwu_handler_->handle_initialize_calls()[0].service_id);
  EXPECT_EQ(
      kEndpointId1,
      fake_web_rtc_bwu_handler_->handle_initialize_calls()[0].endpoint_id);

  // Establish the incoming connection on the new medium. Verify that the
  // upgrade channel replaces the initial channel.
  std::shared_ptr<EndpointChannel> shared_initial_channel =
      ecm_.GetChannelForEndpoint(std::string(kEndpointId1));
  EXPECT_EQ(initial_channel, shared_initial_channel.get());
  FakeEndpointChannel* upgraded_channel =
      fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
          /*initialize_call_index=*/0u, bwu_manager_.get());
  EXPECT_EQ(upgraded_channel,
            ecm_.GetChannelForEndpoint(std::string(kEndpointId1)).get());

  // Confirm that upgrade channel is paused until initial channel is shut down.
  EXPECT_TRUE(upgraded_channel->IsPaused());
  EXPECT_FALSE(initial_channel->is_closed());

  // Receive BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL and then
  // BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL from the
  // Responder device to trigger the shutdown of the initial Bluetooth channel.
  ExceptionOr<OfflineFrame> last_write_frame =
      parser::FromBytes(parser::ForBwuLastWrite());
  bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);
  ExceptionOr<OfflineFrame> safe_to_close_frame =
      parser::FromBytes(parser::ForBwuSafeToClose());
  bwu_manager_->OnIncomingFrame(safe_to_close_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  // Confirm that upgrade channel is resumed after initial channel is shut down.
  // Note: If we didn't grab the shared initial channel pointer above, this
  // channel would have already been destroyed.
  auto old_channel =
      dynamic_cast<FakeEndpointChannel*>(shared_initial_channel.get());
  EXPECT_FALSE(upgraded_channel->IsPaused());
  EXPECT_TRUE(old_channel->is_closed());
  EXPECT_EQ(proto::connections::DisconnectionReason::UPGRADED,
            old_channel->disconnection_reason());
}

TEST_P(BwuManagerTestParam,
       InitiateBwu_Error_DontUpgradeIfAlreadyConenctedOverTheRequestedMedium) {
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());

  // Ignore request to upgrade to WebRTC if we're already connected.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());
}

TEST_P(BwuManagerTestParam, InitiateBwu_Error_NoMediumHandler) {
  // Try to upgrade to a medium without a handler (WIFI_HOTSPOT is not support
  // in these tests). Should just early return with no action.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_HOTSPOT);

  // Make sure none of the other medium handlers are called.
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
}

TEST_P(BwuManagerTestParam, InitiateBwu_Error_UpgradeAlreadyInProgress) {
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());

  // Try to upgrade an endpoint that already has an ungrade in progress. Should
  // just early return with no action.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_LAN);
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
}

TEST_P(BwuManagerTestParam,
       InitiateBwu_Error_FailedToWriteUpgradePathAvailableFrame) {
  // Create the initial device-to-device Bluetooth connection.
  FakeEndpointChannel* initial_channel =
      CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  // Make the initial endpoint channel fail when writing the
  // UPGRADE_PATH_AVAILABLE frame.
  initial_channel->set_write_output(Exception{Exception::kIo});

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);

  // After we notify the WebRTC handler, we try to write the
  // UPGRADE_PATH_AVAILABLE frame, but fail by just early returning.
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());

  // However, we do not record an in-progress attempt. So, if we see an incoming
  // connection over WebRTC, we ignore it. In other words, the initial BLUETOOTH
  // channel is still used.
  EXPECT_EQ(initial_channel,
            ecm_.GetChannelForEndpoint(std::string(kEndpointId1)).get());
  FakeEndpointChannel* upgraded_channel =
      fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
          /*initialize_call_index=*/0u, bwu_manager_.get());
  EXPECT_NE(upgraded_channel,
            ecm_.GetChannelForEndpoint(std::string(kEndpointId1)).get());
  EXPECT_EQ(initial_channel,
            ecm_.GetChannelForEndpoint(std::string(kEndpointId1)).get());
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleEndpoints_FlagEnabled) {
  FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums = true;

  // Say we have two already upgraded WebRTC connections for the same service.
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  std::string upgrade_service_id = WrapInitiatorUpgradeServiceId(kServiceIdA);

  EXPECT_TRUE(fake_web_rtc_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  {
    // Disconnect the first WebRTC endpoint. We don't expect a revert until the
    // last WebRTC endpoint for the service is disconnected.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId1));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id,
                                       std::string(kEndpointId1), latch);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_web_rtc_bwu_handler_->disconnect_calls()[0].endpoint_id);
    EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  }
  {
    // Disconnect the second WebRTC endpoint. We expect a revert.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId2));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id,
                                       std::string(kEndpointId2), latch);
    ASSERT_EQ(2u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId2,
              fake_web_rtc_bwu_handler_->disconnect_calls()[1].endpoint_id);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id,
              fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);
  }
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleEndpoints_FlagDisabled) {
  FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums =
      false;

  // Say we have two already upgraded WebRTC connections for the same service.
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  std::string upgrade_service_id = WrapInitiatorUpgradeServiceId(kServiceIdA);

  EXPECT_TRUE(fake_web_rtc_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  {
    // Disconnect the first WebRTC endpoint.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId1));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id,
                                       std::string(kEndpointId1), latch);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_web_rtc_bwu_handler_->disconnect_calls()[0].endpoint_id);

    // Note(nohle): There appears to be an off-by-one error in the
    // existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id,
              fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);
  }
  {
    // Disconnect the second WebRTC endpoint.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId2));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id,
                                       std::string(kEndpointId2), latch);

    // Note(nohle): There appears to be an off-by-one error in the
    // existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    // The WebRTC medium was already reverted, so we don't expect any more
    // disconnect or revert calls to be processed for WebRTC.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
  }
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleServices_FlagEnabled) {
  FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums = true;

  // Say we have two already upgraded WLAN connections for different services.
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdB, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);

  std::string upgrade_service_id_A = WrapInitiatorUpgradeServiceId(kServiceIdA);
  std::string upgrade_service_id_B = WrapInitiatorUpgradeServiceId(kServiceIdB);

  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
  {
    CountDownLatch latch(1);
    EXPECT_EQ(2u, ecm_.GetConnectedEndpointsCount());
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId1));
    EXPECT_EQ(1u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_A,
                                       std::string(kEndpointId1), latch);
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[0].endpoint_id);

      // With the support_multiple_bwu_mediums flag enabled, we have more
      // granular per-service tracking. So, we can revert for each service when
      // the last endpoint of that medium for the service goes down.
      ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
      EXPECT_EQ(
          upgrade_service_id_A,
          fake_wifi_lan_bwu_handler_->handle_revert_calls()[0].service_id);
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId2));
    EXPECT_EQ(0u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_B,
                                       std::string(kEndpointId2), latch);
    ASSERT_EQ(2u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId2,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[1].endpoint_id);
    EXPECT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
  }
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleServices_FlagDisabled) {
  FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums =
      false;

  // Say we have two already upgraded WLAN connections for different services.
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdB, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);

  std::string upgrade_service_id_A = WrapInitiatorUpgradeServiceId(kServiceIdA);
  std::string upgrade_service_id_B = WrapInitiatorUpgradeServiceId(kServiceIdB);

  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
  {
    CountDownLatch latch(1);
    EXPECT_EQ(2u, ecm_.GetConnectedEndpointsCount());
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId1));
    EXPECT_EQ(1u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_A,
                                       std::string(kEndpointId1), latch);
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[0].endpoint_id);

    // With the flag disabled, we only look at the _total_ number of connected
    // endpoints and revert _all_ services when all endpoints are
    // disconnected. Note(nohle): There appears to be an off-by-one error in
    // the existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    EXPECT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId2));
    EXPECT_EQ(0u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_B,
                                       std::string(kEndpointId2), latch);
    // Note(nohle): There appears to be an off-by-one error in the
    // existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    // We already reverted for _all_ services.
    EXPECT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
  }
}

TEST_F(
    BwuManagerTest,
    InitiateBwu_Revert_OnDisconnect_MultipleServicesAndEndpoints_FlagEnabled) {
  // Need support_multiple_bwu_mediums_ to run this test with multiple mediums.
  FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums = true;

  // Say we have three upgraded connections for two different services and two
  // different mediums.
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdB, kEndpointId3, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdB, kEndpointId4, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);
  FullyUpgradeEndpoint(kEndpointId3, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);
  FullyUpgradeEndpoint(kEndpointId4, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_HOTSPOT);

  std::string upgrade_service_id_A = WrapInitiatorUpgradeServiceId(kServiceIdA);
  std::string upgrade_service_id_B = WrapInitiatorUpgradeServiceId(kServiceIdB);

  // Verify that the medium's BWU handler only gets notified to revert the
  // medium--for example, stop accepting connections on the socket--once every
  // endpoint of that medium for the service is disconnected.
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
  EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->handle_revert_calls().empty());
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId1));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_A,
                                       std::string(kEndpointId1), latch);

    // No more WebRTC channels for service A; expect revert call.
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_web_rtc_bwu_handler_->disconnect_calls()[0].endpoint_id);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id_A,
              fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);

    // We reverted a WebRTC channel; no WLAN calls expected.
    EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
    EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
    EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->disconnect_calls().empty());
    EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->handle_revert_calls().empty());
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId2));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_A,
                                       std::string(kEndpointId2), latch);

    // We reverted a WLAN channel; no additional WebRTC calls expected.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());

    // No more WLAN channels for service A; expect revert call.
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId2,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[0].endpoint_id);
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id_A,
              fake_wifi_lan_bwu_handler_->handle_revert_calls()[0].service_id);
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId3));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_B,
                                       std::string(kEndpointId3), latch);

    // We reverted a WLAN channel; no additional WebRTC calls expected.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());

    // No more WLAN channels for service B; expect revert call.
    ASSERT_EQ(2u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId3,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[1].endpoint_id);
    ASSERT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id_B,
              fake_wifi_lan_bwu_handler_->handle_revert_calls()[1].service_id);
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(std::string(kEndpointId4));
    bwu_manager_->OnEndpointDisconnect(&client_, upgrade_service_id_B,
                                       std::string(kEndpointId4), latch);

    // We reverted a Hotspot channel; no additional WebRTC calls expected.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());

    // No more Hotspot channels for service B; expect revert call.
    ASSERT_EQ(1u, fake_wifi_hotspot_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(
        kEndpointId4,
        fake_wifi_hotspot_bwu_handler_->disconnect_calls()[0].endpoint_id);
    ASSERT_EQ(1u, fake_wifi_hotspot_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(
        upgrade_service_id_B,
        fake_wifi_hotspot_bwu_handler_->handle_revert_calls()[0].service_id);
  }
}

TEST_F(BwuManagerTest, InitiateBwu_Revert_OnUpgradeFailure_FlagEnabled) {
  FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums = true;

  // Say we have two already upgraded WebRTC connections for service A.
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  // Service B has an initial Bluetooth connection that it tries to upgrade.
  CreateInitialEndpoint(kServiceIdB, kEndpointId3, Medium::BLUETOOTH);
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId3),
                                       Medium::WEB_RTC);
  fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
      /*initialize_call_index=*/2u, bwu_manager_.get());

  // This upgrade fails.
  BwuHandler::UpgradePathInfo info;
  info.set_medium(BwuHandler::UpgradePathInfo::WEB_RTC);
  ExceptionOr<OfflineFrame> upgrade_failure =
      parser::FromBytes(parser::ForBwuFailure(info));
  bwu_manager_->OnIncomingFrame(upgrade_failure.result(),
                                std::string(kEndpointId3), &client_,
                                Medium::WEB_RTC);

    // With the flag enabled, we can safely revert WebRTC just for service B
    // because service B has no active WebRTC endpoints.
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(WrapInitiatorUpgradeServiceId(kServiceIdB),
              fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);
}

TEST_F(BwuManagerTest, InitiateBwu_Revert_OnUpgradeFailure_FlagDisabled) {
  FeatureFlags::GetMutableFlagsForTesting().support_multiple_bwu_mediums =
      false;

  // Say we have two already upgraded WebRTC connections for service A.
  CreateInitialEndpoint(kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  // Service B has an initial Bluetooth connection that it tries to upgrade.
  CreateInitialEndpoint(kServiceIdB, kEndpointId3, Medium::BLUETOOTH);
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId3),
                                       Medium::WEB_RTC);
  fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
      /*initialize_call_index=*/2u, bwu_manager_.get());

  // This upgrade fails.
  BwuHandler::UpgradePathInfo info;
  info.set_medium(BwuHandler::UpgradePathInfo::WEB_RTC);
  ExceptionOr<OfflineFrame> upgrade_failure =
      parser::FromBytes(parser::ForBwuFailure(info));
  bwu_manager_->OnIncomingFrame(upgrade_failure.result(),
                                std::string(kEndpointId3), &client_,
                                Medium::WEB_RTC);

  // With the flag disabled, we don't revert if there are still connected
  // endpoints for _any_ service. We don't have service-level bookkeeping; we
  // only know that there is some active WebRTC endpoint.
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
}

INSTANTIATE_TEST_SUITE_P(BwuManagerTestParam, BwuManagerTestParam,
                         testing::Bool());

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
