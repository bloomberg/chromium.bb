// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_MESSAGE_H_
#define CAST_STREAMING_RECEIVER_MESSAGE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/variant.h"
#include "cast/streaming/answer_messages.h"
#include "json/value.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

struct ReceiverWifiStatus {
  Json::Value ToJson() const;
  static ErrorOr<ReceiverWifiStatus> Parse(const Json::Value& value);

  // Current WiFi signal to noise ratio in decibels.
  double wifi_snr = 0.0;

  // Min, max, average, and current bandwidth in bps in order of the WiFi link.
  // Example: [1200, 1300, 1250, 1230].
  std::vector<int32_t> wifi_speed;
};

struct ReceiverCapability {
  static constexpr int kRemotingVersionUnknown = -1;

  Json::Value ToJson() const;
  static ErrorOr<ReceiverCapability> Parse(const Json::Value& value);

  // The remoting version that the receiver uses.
  int remoting_version = kRemotingVersionUnknown;

  // Set of capabilities (e.g., ac3, 4k, hevc, vp9, dolby_vision, etc.).
  std::vector<std::string> media_capabilities;
};

struct ReceiverError {
  Json::Value ToJson() const;
  static ErrorOr<ReceiverError> Parse(const Json::Value& value);

  // Error code.
  int32_t code = -1;

  // Error description.
  std::string description;
};

struct ReceiverMessage {
 public:
  // Receiver response message type.
  enum class Type {
    // Unknown message type.
    kUnknown,

    // Response to OFFER message.
    kAnswer,

    // Response to GET_STATUS message.
    kStatusResponse,

    // Response to GET_CAPABILITIES message.
    kCapabilitiesResponse,

    // Rpc binary messages. The payload is base64-encoded.
    kRpc,
  };

  static ErrorOr<ReceiverMessage> Parse(const Json::Value& value);
  ErrorOr<Json::Value> ToJson() const;

  Type type = Type::kUnknown;

  int32_t sequence_number = -1;

  bool valid = false;

  absl::variant<absl::monostate,
                Answer,
                std::string,
                ReceiverWifiStatus,
                ReceiverCapability,
                ReceiverError>
      body;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_RECEIVER_MESSAGE_H_
