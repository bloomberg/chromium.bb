// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_message_receiver.h"

#include "chromeos/components/eche_app_ui/proto/exo_messages.pb.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace eche_app {
class FakeObserver : public EcheMessageReceiver::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t apps_access_state_response_num_calls() const {
    return apps_access_state_response_num_calls_;
  }

  size_t apps_setup_response_num_calls() const { return apps_setup_response_; }

  proto::GetAppsAccessStateResponse get_last_apps_access_state() const {
    return last_apps_access_state_response_;
  }

  proto::SendAppsSetupResponse get_last_apps_setup_response() const {
    return last_apps_setup_reponse_;
  }

  // EcheMessageReceiver::Observer:
  void onGetAppsAccessStateResponseReceived(
      proto::GetAppsAccessStateResponse apps_access_state_response) override {
    last_apps_access_state_response_ = apps_access_state_response;
    ++apps_access_state_response_num_calls_;
  }

  void onSendAppsSetupResponseReceived(
      proto::SendAppsSetupResponse apps_setup_response) override {
    last_apps_setup_reponse_ = apps_setup_response;
    ++apps_setup_response_;
  }

 private:
  size_t apps_access_state_response_num_calls_ = 0;
  size_t apps_setup_response_ = 0;
  proto::GetAppsAccessStateResponse last_apps_access_state_response_;
  proto::SendAppsSetupResponse last_apps_setup_reponse_;
};

class EcheMessageReceiverTest : public testing::Test {
 protected:
  EcheMessageReceiverTest()
      : fake_connection_manager_(
            std::make_unique<secure_channel::FakeConnectionManager>()) {}
  EcheMessageReceiverTest(const EcheMessageReceiverTest&) = delete;
  EcheMessageReceiverTest& operator=(const EcheMessageReceiverTest&) = delete;
  ~EcheMessageReceiverTest() override = default;

  void SetUp() override {
    message_receiver_ =
        std::make_unique<EcheMessageReceiver>(fake_connection_manager_.get());
    message_receiver_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    message_receiver_->RemoveObserver(&fake_observer_);
  }

  size_t GetNumAppsAccessStateResponseCalls() const {
    return fake_observer_.apps_access_state_response_num_calls();
  }

  size_t GetNumAppsSetupResponseCalls() const {
    return fake_observer_.apps_setup_response_num_calls();
  }

  proto::GetAppsAccessStateResponse GetLastAppsAccessState() const {
    return fake_observer_.get_last_apps_access_state();
  }

  proto::SendAppsSetupResponse GetLastAppsSetupResponse() const {
    return fake_observer_.get_last_apps_setup_response();
  }

  FakeObserver fake_observer_;
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<EcheMessageReceiver> message_receiver_;
};

TEST_F(EcheMessageReceiverTest, onGetAppsAccessStateResponseReceived) {
  proto::GetAppsAccessStateResponse response;
  response.set_result(eche_app::proto::Result::RESULT_ERROR_ACTION_FAILED);
  response.set_apps_access_state(
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  proto::ExoMessage message;
  *message.mutable_apps_access_state_response() = std::move(response);

  fake_connection_manager_->NotifyMessageReceived(message.SerializeAsString());

  proto::GetAppsAccessStateResponse actual_apps_state =
      GetLastAppsAccessState();

  EXPECT_EQ(1u, GetNumAppsAccessStateResponseCalls());
  EXPECT_EQ(0u, GetNumAppsSetupResponseCalls());
  EXPECT_EQ(eche_app::proto::Result::RESULT_ERROR_ACTION_FAILED,
            actual_apps_state.result());
  EXPECT_EQ(eche_app::proto::AppsAccessState::ACCESS_GRANTED,
            actual_apps_state.apps_access_state());
}

TEST_F(EcheMessageReceiverTest, onSendAppsSetupResponseReceived) {
  proto::SendAppsSetupResponse response;
  response.set_result(eche_app::proto::Result::RESULT_ERROR_ACTION_FAILED);
  response.set_apps_access_state(
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  proto::ExoMessage message;
  *message.mutable_apps_setup_response() = std::move(response);

  fake_connection_manager_->NotifyMessageReceived(message.SerializeAsString());

  proto::SendAppsSetupResponse actual_apps_setup_response =
      GetLastAppsSetupResponse();

  EXPECT_EQ(0u, GetNumAppsAccessStateResponseCalls());
  EXPECT_EQ(1u, GetNumAppsSetupResponseCalls());
  EXPECT_EQ(eche_app::proto::Result::RESULT_ERROR_ACTION_FAILED,
            actual_apps_setup_response.result());
  EXPECT_EQ(eche_app::proto::AppsAccessState::ACCESS_GRANTED,
            actual_apps_setup_response.apps_access_state());
}

}  // namespace eche_app
}  // namespace chromeos
