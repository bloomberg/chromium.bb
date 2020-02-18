// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_SENDER_H_
#define CAST_COMMON_MDNS_MDNS_SENDER_H_

#include "cast/common/mdns/mdns_records.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace cast {
namespace mdns {

using Error = openscreen::Error;
using UdpSocket = openscreen::platform::UdpSocket;
using IPEndpoint = openscreen::IPEndpoint;

class MdnsSender {
 public:
  explicit MdnsSender(UdpSocket* socket);
  MdnsSender(const MdnsSender& other) = delete;
  MdnsSender(MdnsSender&& other) noexcept = default;
  ~MdnsSender() = default;

  MdnsSender& operator=(const MdnsSender& other) = delete;
  MdnsSender& operator=(MdnsSender&& other) noexcept = default;

  Error SendMulticast(const MdnsMessage& message);
  Error SendUnicast(const MdnsMessage& message, const IPEndpoint& endpoint);

 private:
  UdpSocket* socket_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_SENDER_H_
