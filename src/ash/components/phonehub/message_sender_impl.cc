// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/message_sender_impl.h"

#include <netinet/in.h>

#include "ash/components/phonehub/util/histogram_util.h"
#include "ash/constants/ash_features.h"
#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {
namespace phonehub {

namespace {

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

MessageSenderImpl::MessageSenderImpl(
    secure_channel::ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {
  DCHECK(connection_manager_);
}

MessageSenderImpl::~MessageSenderImpl() = default;

void MessageSenderImpl::SendCrosState(bool notification_setting_enabled,
                                      bool camera_roll_setting_enabled) {
  proto::NotificationSetting is_notification_enabled =
      notification_setting_enabled
          ? proto::NotificationSetting::NOTIFICATIONS_ON
          : proto::NotificationSetting::NOTIFICATIONS_OFF;
  proto::CameraRollSetting is_camera_roll_enabled =
      camera_roll_setting_enabled ? proto::CameraRollSetting::CAMERA_ROLL_ON
                                  : proto::CameraRollSetting::CAMERA_ROLL_OFF;
  proto::CrosState request;
  request.set_notification_setting(is_notification_enabled);
  request.set_camera_roll_setting(is_camera_roll_enabled);
  if (features::IsPhoneHubMonochromeNotificationIconsEnabled()) {
    // Updated Chromebooks should always use the new flag, but a flag is still
    // necessary to identify end-of-support Chromebooks so the phone can know
    // to send backwards-compatible messages.
    request.set_notification_icon_styling(
        proto::NotificationIconStyling::ICON_STYLE_MONOCHROME_SMALL_ICON);
  }

  SendMessage(proto::MessageType::PROVIDE_CROS_STATE, &request);
}

void MessageSenderImpl::SendUpdateNotificationModeRequest(
    bool do_not_disturb_enabled) {
  proto::NotificationMode notification_mode =
      do_not_disturb_enabled ? proto::NotificationMode::DO_NOT_DISTURB_ON
                             : proto::NotificationMode::DO_NOT_DISTURB_OFF;
  proto::UpdateNotificationModeRequest request;
  request.set_notification_mode(notification_mode);

  SendMessage(proto::MessageType::UPDATE_NOTIFICATION_MODE_REQUEST, &request);
}

void MessageSenderImpl::SendUpdateBatteryModeRequest(
    bool battery_saver_mode_enabled) {
  proto::BatteryMode battery_mode = battery_saver_mode_enabled
                                        ? proto::BatteryMode::BATTERY_SAVER_ON
                                        : proto::BatteryMode::BATTERY_SAVER_OFF;
  proto::UpdateBatteryModeRequest request;
  request.set_battery_mode(battery_mode);

  SendMessage(proto::MessageType::UPDATE_BATTERY_MODE_REQUEST, &request);
}

void MessageSenderImpl::SendDismissNotificationRequest(
    int64_t notification_id) {
  proto::DismissNotificationRequest request;
  request.set_notification_id(notification_id);

  SendMessage(proto::MessageType::DISMISS_NOTIFICATION_REQUEST, &request);
}

void MessageSenderImpl::SendNotificationInlineReplyRequest(
    int64_t notification_id,
    const std::u16string& reply_text) {
  proto::NotificationInlineReplyRequest request;
  request.set_notification_id(notification_id);
  request.set_reply_text(base::UTF16ToUTF8(reply_text));

  SendMessage(proto::MessageType::NOTIFICATION_INLINE_REPLY_REQUEST, &request);
}

void MessageSenderImpl::SendShowNotificationAccessSetupRequest() {
  proto::ShowNotificationAccessSetupRequest request;

  SendMessage(proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_REQUEST,
              &request);
}

void MessageSenderImpl::SendFeatureSetupRequest(bool camera_roll,
                                                bool notifications) {
  proto::FeatureSetupRequest request;
  request.set_camera_roll_setup_requested(camera_roll);
  request.set_notification_setup_requested(notifications);

  SendMessage(proto::MessageType::FEATURE_SETUP_REQUEST, &request);
}

void MessageSenderImpl::SendRingDeviceRequest(bool device_ringing_enabled) {
  proto::FindMyDeviceRingStatus ringing_enabled =
      device_ringing_enabled ? proto::FindMyDeviceRingStatus::RINGING
                             : proto::FindMyDeviceRingStatus::NOT_RINGING;
  proto::RingDeviceRequest request;
  request.set_ring_status(ringing_enabled);

  SendMessage(proto::MessageType::RING_DEVICE_REQUEST, &request);
}

void MessageSenderImpl::SendFetchCameraRollItemsRequest(
    const proto::FetchCameraRollItemsRequest& request) {
  SendMessage(proto::MessageType::FETCH_CAMERA_ROLL_ITEMS_REQUEST, &request);
}

void MessageSenderImpl::SendFetchCameraRollItemDataRequest(
    const proto::FetchCameraRollItemDataRequest& request) {
  SendMessage(proto::MessageType::FETCH_CAMERA_ROLL_ITEM_DATA_REQUEST,
              &request);
}

void MessageSenderImpl::SendInitiateCameraRollItemTransferRequest(
    const proto::InitiateCameraRollItemTransferRequest& request) {
  SendMessage(proto::MessageType::INITIATE_CAMERA_ROLL_ITEM_TRANSFER_REQUEST,
              &request);
}

void MessageSenderImpl::SendMessage(
    proto::MessageType message_type,
    const google::protobuf::MessageLite* request) {
  connection_manager_->SendMessage(SerializeMessage(message_type, request));
  UMA_HISTOGRAM_ENUMERATION("PhoneHub.Usage.SentMessageTypeCount", message_type,
                            proto::MessageType_MAX);
  util::LogMessageResult(message_type,
                         util::PhoneHubMessageResult::kRequestAttempted);
}

}  // namespace phonehub
}  // namespace ash
