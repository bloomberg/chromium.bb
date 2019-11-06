// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_sender.h"

#include "cast/common/mdns/mdns_writer.h"

namespace cast {
namespace mdns {

namespace {

const IPEndpoint& GetIPv6MdnsMulticastEndpoint() {
  static IPEndpoint endpoint{.address = IPAddress(kDefaultMulticastGroupIPv6),
                             .port = kDefaultMulticastPort};
  return endpoint;
}

const IPEndpoint& GetIPv4MdnsMulticastEndpoint() {
  static IPEndpoint endpoint{.address = IPAddress(kDefaultMulticastGroupIPv4),
                             .port = kDefaultMulticastPort};
  return endpoint;
}

}  // namespace

MdnsSender::MdnsSender(UdpSocket* socket) : socket_(socket) {
  OSP_DCHECK(socket_ != nullptr);
}

Error MdnsSender::SendMulticast(const MdnsMessage& message) {
  const IPEndpoint& endpoint = socket_->IsIPv6()
                                   ? GetIPv6MdnsMulticastEndpoint()
                                   : GetIPv4MdnsMulticastEndpoint();
  return SendUnicast(message, endpoint);
}

Error MdnsSender::SendUnicast(const MdnsMessage& message,
                              const IPEndpoint& endpoint) {
  // Always try to write the message into the buffer even if MaxWireSize is
  // greater than maximum message size. Domain name compression might reduce the
  // on-the-wire size of the message sufficiently for it to fit into the buffer.
  std::vector<uint8_t> buffer(
      std::min(message.MaxWireSize(), kMaxMulticastMessageSize));
  MdnsWriter writer(buffer.data(), buffer.size());
  if (!writer.Write(message)) {
    return Error::Code::kInsufficientBuffer;
  }
  return socket_->SendMessage(buffer.data(), writer.offset(), endpoint);
}

}  // namespace mdns
}  // namespace cast
