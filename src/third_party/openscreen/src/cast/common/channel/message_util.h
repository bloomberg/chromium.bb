// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
#define CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_

#include "absl/strings/string_view.h"
#include "cast/common/channel/proto/cast_channel.pb.h"

namespace cast {
namespace channel {

// Reserved message namespaces for internal messages.
static constexpr char kCastInternalNamespacePrefix[] =
    "urn:x-cast:com.google.cast.";
static constexpr char kTransportNamespacePrefix[] =
    "urn:x-cast:com.google.cast.tp.";
static constexpr char kAuthNamespace[] =
    "urn:x-cast:com.google.cast.tp.deviceauth";
static constexpr char kHeartbeatNamespace[] =
    "urn:x-cast:com.google.cast.tp.heartbeat";
static constexpr char kConnectionNamespace[] =
    "urn:x-cast:com.google.cast.tp.connection";
static constexpr char kReceiverNamespace[] =
    "urn:x-cast:com.google.cast.receiver";
static constexpr char kBroadcastNamespace[] =
    "urn:x-cast:com.google.cast.broadcast";
static constexpr char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";

// Sender and receiver IDs to use for platform messages.
static constexpr char kPlatformSenderId[] = "sender-0";
static constexpr char kPlatformReceiverId[] = "receiver-0";

inline bool IsAuthMessage(const CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

inline bool IsTransportNamespace(absl::string_view namespace_) {
  return (namespace_.size() > (sizeof(kTransportNamespacePrefix) - 1)) &&
         (namespace_.find_first_of(kTransportNamespacePrefix) == 0);
}

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
