// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RECEIVER_H_
#define DISCOVERY_MDNS_MDNS_RECEIVER_H_

#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/udp_packet.h"

namespace openscreen {
namespace discovery {

class MdnsMessage;

class MdnsReceiver : openscreen::platform::UdpSocket::Client {
 public:
  using UdpPacket = openscreen::platform::UdpPacket;
  using UdpSocket = openscreen::platform::UdpSocket;

  // MdnsReceiver does not own |socket| and |delegate|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsReceiver.
  explicit MdnsReceiver(UdpSocket* socket);
  MdnsReceiver(const MdnsReceiver& other) = delete;
  MdnsReceiver(MdnsReceiver&& other) noexcept = delete;
  MdnsReceiver& operator=(const MdnsReceiver& other) = delete;
  MdnsReceiver& operator=(MdnsReceiver&& other) noexcept = delete;
  ~MdnsReceiver() override;

  void SetQueryCallback(
      std::function<void(const MdnsMessage&, const IPEndpoint& src)> callback);
  void SetResponseCallback(std::function<void(const MdnsMessage&)> callback);

  // The receiver can be started and stopped multiple times.
  // Start and Stop are both synchronous calls. When MdnsReceiver has not yet
  // been started, OnRead callbacks it receives from the task runner will be
  // no-ops.
  void Start();
  void Stop();

  // UdpSocket::Client overrides.
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;
  void OnError(UdpSocket* socket, Error error) override;
  void OnSendError(UdpSocket* socket, Error error) override;

 private:
  enum class State {
    kStopped,
    kRunning,
  };

  UdpSocket* const socket_;
  std::function<void(const MdnsMessage&, const IPEndpoint& src)>
      query_callback_;
  std::function<void(const MdnsMessage&)> response_callback_;
  State state_ = State::kStopped;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_RECEIVER_H_
