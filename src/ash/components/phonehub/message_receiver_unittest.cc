// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/message_receiver_impl.h"

#include <netinet/in.h>
#include <memory>

#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

class FakeObserver : public MessageReceiver::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t snapshot_num_calls() const {
    return phone_status_snapshot_updated_num_calls_;
  }

  size_t status_updated_num_calls() const {
    return phone_status_updated_num_calls_;
  }

  size_t fetch_camera_roll_items_response_calls() const {
    return fetch_camera_roll_items_response_calls_;
  }

  size_t fetch_camera_roll_item_data_response_calls() const {
    return fetch_camera_roll_item_data_response_calls_;
  }

  proto::PhoneStatusSnapshot last_snapshot() const { return last_snapshot_; }

  proto::PhoneStatusUpdate last_status_update() const {
    return last_status_update_;
  }

  proto::FetchCameraRollItemsResponse last_fetch_camera_roll_items_response()
      const {
    return last_fetch_camera_roll_items_response_;
  }

  proto::FetchCameraRollItemDataResponse
  last_fetch_camera_roll_item_data_response() const {
    return last_fetch_camera_roll_item_data_response_;
  }

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override {
    last_snapshot_ = phone_status_snapshot;
    ++phone_status_snapshot_updated_num_calls_;
  }

  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override {
    last_status_update_ = phone_status_update;
    ++phone_status_updated_num_calls_;
  }

  void OnFetchCameraRollItemsResponseReceived(
      const proto::FetchCameraRollItemsResponse& response) override {
    last_fetch_camera_roll_items_response_ = response;
    ++fetch_camera_roll_items_response_calls_;
  }

  void OnFetchCameraRollItemDataResponseReceived(
      const proto::FetchCameraRollItemDataResponse& response) override {
    last_fetch_camera_roll_item_data_response_ = response;
    ++fetch_camera_roll_item_data_response_calls_;
  }

 private:
  size_t phone_status_snapshot_updated_num_calls_ = 0;
  size_t phone_status_updated_num_calls_ = 0;
  size_t fetch_camera_roll_items_response_calls_ = 0;
  size_t fetch_camera_roll_item_data_response_calls_ = 0;
  proto::PhoneStatusSnapshot last_snapshot_;
  proto::PhoneStatusUpdate last_status_update_;
  proto::FetchCameraRollItemsResponse last_fetch_camera_roll_items_response_;
  proto::FetchCameraRollItemDataResponse
      last_fetch_camera_roll_item_data_response_;
};

std::string SerializeMessage(proto::MessageType message_type,
                             const google::protobuf::MessageLite* request) {
  // Add two space characters, followed by the serialized proto.
  std::string message = base::StrCat({"  ", request->SerializeAsString()});

  // Replace the first two characters with |message_type| as a 16-bit int.
  uint16_t* ptr =
      reinterpret_cast<uint16_t*>(const_cast<char*>(message.data()));
  *ptr = htons(static_cast<uint16_t>(message_type));
  return message;
}

}  // namespace

class MessageReceiverImplTest : public testing::Test {
 protected:
  MessageReceiverImplTest()
      : fake_connection_manager_(
            std::make_unique<secure_channel::FakeConnectionManager>()) {}
  MessageReceiverImplTest(const MessageReceiverImplTest&) = delete;
  MessageReceiverImplTest& operator=(const MessageReceiverImplTest&) = delete;
  ~MessageReceiverImplTest() override = default;

  void SetUp() override {
    message_receiver_ =
        std::make_unique<MessageReceiverImpl>(fake_connection_manager_.get());
    message_receiver_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    message_receiver_->RemoveObserver(&fake_observer_);
  }

  size_t GetNumPhoneStatusSnapshotCalls() const {
    return fake_observer_.snapshot_num_calls();
  }

  size_t GetNumPhoneStatusUpdatedCalls() const {
    return fake_observer_.status_updated_num_calls();
  }

  size_t GetNumFetchCameraRollItemsResponseCalls() const {
    return fake_observer_.fetch_camera_roll_items_response_calls();
  }

  size_t GetNumFetchCameraRollItemDataResponseCalls() const {
    return fake_observer_.fetch_camera_roll_item_data_response_calls();
  }

  proto::PhoneStatusSnapshot GetLastSnapshot() const {
    return fake_observer_.last_snapshot();
  }

  proto::PhoneStatusUpdate GetLastStatusUpdate() const {
    return fake_observer_.last_status_update();
  }

  proto::FetchCameraRollItemsResponse GetLastFetchCameraRollItemsResponse()
      const {
    return fake_observer_.last_fetch_camera_roll_items_response();
  }

  proto::FetchCameraRollItemDataResponse
  GetLastFetchCameraRollItemDataResponse() const {
    return fake_observer_.last_fetch_camera_roll_item_data_response();
  }

  FakeObserver fake_observer_;
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<MessageReceiverImpl> message_receiver_;
};

TEST_F(MessageReceiverImplTest, OnPhoneStatusSnapshotReceieved) {
  const int32_t expected_battery_percentage = 15;
  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_battery_percentage(
      expected_battery_percentage);

  proto::PhoneStatusSnapshot expected_snapshot;
  expected_snapshot.set_allocated_properties(
      expected_phone_properties.release());
  expected_snapshot.add_notifications();

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::PHONE_STATUS_SNAPSHOT, &expected_snapshot);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::PhoneStatusSnapshot actual_snapshot = GetLastSnapshot();

  EXPECT_EQ(1u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(expected_battery_percentage,
            actual_snapshot.properties().battery_percentage());
  EXPECT_EQ(1, actual_snapshot.notifications_size());
}

TEST_F(MessageReceiverImplTest, OnPhoneStatusUpdated) {
  const int32_t expected_battery_percentage = 15u;
  auto expected_phone_properties = std::make_unique<proto::PhoneProperties>();
  expected_phone_properties->set_battery_percentage(
      expected_battery_percentage);

  proto::PhoneStatusUpdate expected_update;
  expected_update.set_allocated_properties(expected_phone_properties.release());
  expected_update.add_updated_notifications();

  const int64_t expected_removed_id = 24u;
  expected_update.add_removed_notification_ids(expected_removed_id);

  // Simulate receiving a message.
  const std::string expected_message =
      SerializeMessage(proto::PHONE_STATUS_UPDATE, &expected_update);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::PhoneStatusUpdate actual_update = GetLastStatusUpdate();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(1u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(expected_battery_percentage,
            actual_update.properties().battery_percentage());
  EXPECT_EQ(1, actual_update.updated_notifications_size());
  EXPECT_EQ(expected_removed_id, actual_update.removed_notification_ids()[0]);
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemsResponseReceivedWthFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemsResponse expected_response;
  proto::CameraRollItem* item_proto = expected_response.add_items();
  proto::CameraRollItemMetadata* metadata = item_proto->mutable_metadata();
  metadata->set_key("key");
  proto::CameraRollItemThumbnail* thumbnail = item_proto->mutable_thumbnail();
  thumbnail->set_data("encoded_thumbnail_data");

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEMS_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::FetchCameraRollItemsResponse actual_response =
      GetLastFetchCameraRollItemsResponse();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(1u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(1, actual_response.items_size());
  EXPECT_EQ("key", actual_response.items(0).metadata().key());
  EXPECT_EQ("encoded_thumbnail_data",
            actual_response.items(0).thumbnail().data());
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemsResponseReceivedWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemsResponse expected_response;
  proto::CameraRollItem* item_proto = expected_response.add_items();
  proto::CameraRollItemMetadata* metadata = item_proto->mutable_metadata();
  metadata->set_key("key");
  proto::CameraRollItemThumbnail* thumbnail = item_proto->mutable_thumbnail();
  thumbnail->set_data("encoded_thumbnail_data");

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEMS_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemDataResponseReceivedWthFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemDataResponse expected_response;
  expected_response.mutable_metadata()->set_key("key");
  expected_response.set_file_availability(
      proto::FetchCameraRollItemDataResponse::AVAILABLE);
  expected_response.set_payload_id(1234);

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  proto::FetchCameraRollItemDataResponse actual_response =
      GetLastFetchCameraRollItemDataResponse();

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(1u, GetNumFetchCameraRollItemDataResponseCalls());
  EXPECT_EQ("key", actual_response.metadata().key());
  EXPECT_EQ(proto::FetchCameraRollItemDataResponse::AVAILABLE,
            actual_response.file_availability());
  EXPECT_EQ(1234, actual_response.payload_id());
}

TEST_F(MessageReceiverImplTest,
       OnFetchCameraRollItemDataResponseReceivedWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPhoneHubCameraRoll);

  proto::FetchCameraRollItemDataResponse expected_response;
  expected_response.mutable_metadata()->set_key("key");
  expected_response.set_file_availability(
      proto::FetchCameraRollItemDataResponse::AVAILABLE);
  expected_response.set_payload_id(1234);

  // Simulate receiving a message.
  const std::string expected_message = SerializeMessage(
      proto::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE, &expected_response);
  fake_connection_manager_->NotifyMessageReceived(expected_message);

  EXPECT_EQ(0u, GetNumPhoneStatusSnapshotCalls());
  EXPECT_EQ(0u, GetNumPhoneStatusUpdatedCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemsResponseCalls());
  EXPECT_EQ(0u, GetNumFetchCameraRollItemDataResponseCalls());
}

}  // namespace phonehub
}  // namespace ash
