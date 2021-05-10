// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_message.h"

#include <utility>

#include "absl/strings/ascii.h"
#include "cast/streaming/message_fields.h"
#include "json/reader.h"
#include "json/writer.h"
#include "platform/base/error.h"
#include "util/base64.h"
#include "util/enum_name_table.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace {

EnumNameTable<ReceiverMessage::Type, 5> kMessageTypeNames{
    {{kMessageTypeAnswer, ReceiverMessage::Type::kAnswer},
     {"STATUS_RESPONSE", ReceiverMessage::Type::kStatusResponse},
     {"CAPABILITIES_RESPONSE", ReceiverMessage::Type::kCapabilitiesResponse},
     {"RPC", ReceiverMessage::Type::kRpc}}};

ReceiverMessage::Type GetMessageType(const Json::Value& root) {
  std::string type;
  if (!json::ParseAndValidateString(root[kMessageType], &type)) {
    return ReceiverMessage::Type::kUnknown;
  }

  absl::AsciiStrToUpper(&type);
  ErrorOr<ReceiverMessage::Type> parsed = GetEnum(kMessageTypeNames, type);

  return parsed.value(ReceiverMessage::Type::kUnknown);
}

}  // namespace

// static
ErrorOr<ReceiverError> ReceiverError::Parse(const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid,
                 "Empty JSON in receiver error parsing");
  }

  int code;
  std::string description;
  if (!json::ParseAndValidateInt(value[kErrorCode], &code) ||
      !json::ParseAndValidateString(value[kErrorDescription], &description)) {
    return Error::Code::kJsonParseError;
  }
  return ReceiverError{code, description};
}

Json::Value ReceiverError::ToJson() const {
  Json::Value root;
  root[kErrorCode] = code;
  root[kErrorDescription] = description;
  return root;
}

// static
ErrorOr<ReceiverCapability> ReceiverCapability::Parse(
    const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid,
                 "Empty JSON in capabilities parsing");
  }

  int remoting_version;
  if (!json::ParseAndValidateInt(value["remoting"], &remoting_version)) {
    remoting_version = ReceiverCapability::kRemotingVersionUnknown;
  }

  std::vector<std::string> media_capabilities;
  if (!json::ParseAndValidateStringArray(value["mediaCaps"],
                                         &media_capabilities)) {
    return Error(Error::Code::kJsonParseError,
                 "Failed to parse media capabilities");
  }

  return ReceiverCapability{remoting_version, std::move(media_capabilities)};
}

Json::Value ReceiverCapability::ToJson() const {
  Json::Value root;
  root["remoting"] = remoting_version;
  Json::Value capabilities(Json::ValueType::arrayValue);
  for (const auto& capability : media_capabilities) {
    capabilities.append(capability);
  }
  root["mediaCaps"] = std::move(capabilities);
  return root;
}

// static
ErrorOr<ReceiverWifiStatus> ReceiverWifiStatus::Parse(
    const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid,
                 "Empty JSON in status parsing");
  }

  double wifi_snr;
  std::vector<int32_t> wifi_speed;
  if (!json::ParseAndValidateDouble(value["wifiSnr"], &wifi_snr, true) ||
      !json::ParseAndValidateIntArray(value["wifiSpeed"], &wifi_speed)) {
    return Error::Code::kJsonParseError;
  }
  return ReceiverWifiStatus{wifi_snr, std::move(wifi_speed)};
}

Json::Value ReceiverWifiStatus::ToJson() const {
  Json::Value root;
  root["wifiSnr"] = wifi_snr;
  Json::Value speeds(Json::ValueType::arrayValue);
  for (const auto& speed : wifi_speed) {
    speeds.append(speed);
  }
  root["wifiSpeed"] = std::move(speeds);
  return root;
}

// static
ErrorOr<ReceiverMessage> ReceiverMessage::Parse(const Json::Value& value) {
  ReceiverMessage message;
  if (!value || !json::ParseAndValidateInt(value[kSequenceNumber],
                                           &(message.sequence_number))) {
    return Error(Error::Code::kJsonParseError,
                 "Failed to parse sequence number");
  }

  std::string result;
  if (!json::ParseAndValidateString(value[kResult], &result)) {
    result = kResultError;
  }

  message.type = GetMessageType(value);
  message.valid =
      (result == kResultOk || message.type == ReceiverMessage::Type::kRpc);
  if (!message.valid) {
    ErrorOr<ReceiverError> error =
        ReceiverError::Parse(value[kErrorMessageBody]);
    if (error.is_value()) {
      message.body = std::move(error.value());
    }
    return message;
  }

  switch (message.type) {
    case Type::kAnswer: {
      Answer answer;
      if (openscreen::cast::Answer::ParseAndValidate(value[kAnswerMessageBody],
                                                     &answer)) {
        message.body = std::move(answer);
        message.valid = true;
      }
    } break;

    case Type::kStatusResponse: {
      ErrorOr<ReceiverWifiStatus> status =
          ReceiverWifiStatus::Parse(value[kStatusMessageBody]);
      if (status.is_value()) {
        message.body = std::move(status.value());
        message.valid = true;
      }
    } break;

    case Type::kCapabilitiesResponse: {
      ErrorOr<ReceiverCapability> capability =
          ReceiverCapability::Parse(value[kCapabilitiesMessageBody]);
      if (capability.is_value()) {
        message.body = std::move(capability.value());
        message.valid = true;
      }
    } break;

    case Type::kRpc: {
      std::string rpc;
      if (json::ParseAndValidateString(value[kRpcMessageBody], &rpc) &&
          base64::Decode(rpc, &rpc)) {
        message.body = std::move(rpc);
        message.valid = true;
      }
    } break;

    case Type::kUnknown:
    default:
      message.valid = false;
      break;
  }

  return message;
}

ErrorOr<Json::Value> ReceiverMessage::ToJson() const {
  OSP_CHECK(type != ReceiverMessage::Type::kUnknown)
      << "Trying to send an unknown message is a developer error";

  Json::Value root;
  root[kMessageType] = GetEnumName(kMessageTypeNames, type).value();
  if (sequence_number >= 0) {
    root[kSequenceNumber] = sequence_number;
  }

  switch (type) {
    case ReceiverMessage::Type::kAnswer:
      if (valid) {
        root[kResult] = kResultOk;
        root[kAnswerMessageBody] = absl::get<Answer>(body).ToJson();
      } else {
        root[kResult] = kResultError;
        root[kErrorMessageBody] = absl::get<ReceiverError>(body).ToJson();
      }
      break;

    case (ReceiverMessage::Type::kStatusResponse):
      root[kResult] = kResultOk;
      root[kStatusMessageBody] = absl::get<ReceiverWifiStatus>(body).ToJson();
      break;

    case ReceiverMessage::Type::kCapabilitiesResponse:
      root[kResult] = kResultOk;
      root[kCapabilitiesMessageBody] =
          absl::get<ReceiverCapability>(body).ToJson();
      break;

    // NOTE: RPC messages do NOT have a result field.
    case ReceiverMessage::Type::kRpc:
      root[kRpcMessageBody] = base64::Encode(absl::get<std::string>(body));
      break;

    default:
      OSP_NOTREACHED();
  }

  return root;
}

}  // namespace cast
}  // namespace openscreen
