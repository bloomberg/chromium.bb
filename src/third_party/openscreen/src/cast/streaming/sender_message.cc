// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_message.h"

#include <utility>

#include "absl/strings/ascii.h"
#include "cast/streaming/message_fields.h"
#include "util/base64.h"
#include "util/enum_name_table.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace {

EnumNameTable<SenderMessage::Type, 4> kMessageTypeNames{
    {{kMessageTypeOffer, SenderMessage::Type::kOffer},
     {"GET_STATUS", SenderMessage::Type::kGetStatus},
     {"GET_CAPABILITIES", SenderMessage::Type::kGetCapabilities},
     {"RPC", SenderMessage::Type::kRpc}}};

SenderMessage::Type GetMessageType(const Json::Value& root) {
  std::string type;
  if (!json::ParseAndValidateString(root[kMessageType], &type)) {
    return SenderMessage::Type::kUnknown;
  }

  absl::AsciiStrToUpper(&type);
  ErrorOr<SenderMessage::Type> parsed = GetEnum(kMessageTypeNames, type);

  return parsed.value(SenderMessage::Type::kUnknown);
}

}  // namespace

// static
ErrorOr<SenderMessage> SenderMessage::Parse(const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid, "Empty JSON");
  }

  SenderMessage message;
  message.type = GetMessageType(value);
  if (!json::ParseAndValidateInt(value[kSequenceNumber],
                                 &(message.sequence_number))) {
    message.sequence_number = -1;
  }

  if (message.type == SenderMessage::Type::kOffer) {
    ErrorOr<Offer> offer = Offer::Parse(value[kOfferMessageBody]);
    if (offer.is_value()) {
      message.body = std::move(offer.value());
      message.valid = true;
    }
  } else if (message.type == SenderMessage::Type::kRpc) {
    std::string rpc_body;
    std::vector<uint8_t> rpc;
    if (json::ParseAndValidateString(value[kRpcMessageBody], &rpc_body) &&
        base64::Decode(rpc_body, &rpc)) {
      message.body = rpc;
      message.valid = true;
    }
  } else if (message.type == SenderMessage::Type::kGetStatus ||
             message.type == SenderMessage::Type::kGetCapabilities) {
    // These types of messages just don't have a body.
    message.valid = true;
  }

  return message;
}

ErrorOr<Json::Value> SenderMessage::ToJson() const {
  OSP_CHECK(type != SenderMessage::Type::kUnknown)
      << "Trying to send an unknown message is a developer error";

  Json::Value root;
  ErrorOr<const char*> message_type = GetEnumName(kMessageTypeNames, type);
  root[kMessageType] = message_type.value();
  if (sequence_number >= 0) {
    root[kSequenceNumber] = sequence_number;
  }

  switch (type) {
    case SenderMessage::Type::kOffer:
      root[kOfferMessageBody] = absl::get<Offer>(body).ToJson().value();
      break;

    case SenderMessage::Type::kRpc:
      root[kRpcMessageBody] =
          base64::Encode(absl::get<std::vector<uint8_t>>(body));
      break;

    case SenderMessage::Type::kGetCapabilities:  // fallthrough
    case SenderMessage::Type::kGetStatus:
      break;

    default:
      OSP_NOTREACHED();
  }
  return root;
}

}  // namespace cast
}  // namespace openscreen
