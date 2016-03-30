// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message.h"

#include "base/logging.h"

namespace {

// Limit remote security key messages to 256KB.
const uint32_t kMaxSecurityKeyMessageByteCount = 256 * 1024;

const int kRemoteSecurityKeyMessageControlCodeByteCount = 1;

}  // namespace

namespace remoting {

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
  if (message_data.size() > kRemoteSecurityKeyMessageControlCodeByteCount) {
    payload_ = message_data.substr(1);
  }

  return true;
}

}  // namespace remoting