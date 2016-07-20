// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"

namespace {

// Limit remote security key messages to 256KB.
const uint32_t kMaxSecurityKeyMessageByteCount = 256 * 1024;

}  // namespace

namespace remoting {

const int SecurityKeyMessage::kHeaderSizeBytes = 4;

const int SecurityKeyMessage::kMessageTypeSizeBytes = 1;

SecurityKeyMessage::SecurityKeyMessage() {}

SecurityKeyMessage::~SecurityKeyMessage() {}

bool SecurityKeyMessage::IsValidMessageSize(uint32_t message_size) {
  return message_size > 0 && message_size <= kMaxSecurityKeyMessageByteCount;
}

RemoteSecurityKeyMessageType SecurityKeyMessage::MessageTypeFromValue(
    int value) {
  // Note: The static_cast from enum value to int should be safe since the enum
  // type is an unsigned 8bit value.
  switch (value) {
    case static_cast<int>(RemoteSecurityKeyMessageType::CONNECT):
    case static_cast<int>(RemoteSecurityKeyMessageType::CONNECT_RESPONSE):
    case static_cast<int>(RemoteSecurityKeyMessageType::CONNECT_ERROR):
    case static_cast<int>(RemoteSecurityKeyMessageType::REQUEST):
    case static_cast<int>(RemoteSecurityKeyMessageType::REQUEST_RESPONSE):
    case static_cast<int>(RemoteSecurityKeyMessageType::REQUEST_ERROR):
    case static_cast<int>(RemoteSecurityKeyMessageType::UNKNOWN_COMMAND):
    case static_cast<int>(RemoteSecurityKeyMessageType::UNKNOWN_ERROR):
    case static_cast<int>(RemoteSecurityKeyMessageType::INVALID):
      return static_cast<RemoteSecurityKeyMessageType>(value);

    default:
      LOG(ERROR) << "Unknown message type passed in: " << value;
      return RemoteSecurityKeyMessageType::INVALID;
  }
}

std::unique_ptr<SecurityKeyMessage> SecurityKeyMessage::CreateMessageForTest(
    RemoteSecurityKeyMessageType type,
    const std::string& payload) {
  std::unique_ptr<SecurityKeyMessage> message(new SecurityKeyMessage());
  message->type_ = type;
  message->payload_ = payload;

  return message;
}

bool SecurityKeyMessage::ParseMessage(const std::string& message_data) {
  if (!IsValidMessageSize(message_data.size())) {
    return false;
  }

  // The first char of the message is the message type.
  type_ = MessageTypeFromValue(message_data[0]);
  if (type_ == RemoteSecurityKeyMessageType::INVALID) {
    return false;
  }

  payload_.clear();
  if (message_data.size() > kMessageTypeSizeBytes) {
    payload_ = message_data.substr(1);
  }

  return true;
}

}  // namespace remoting
