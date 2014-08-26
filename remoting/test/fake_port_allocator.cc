// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_port_allocator.h"

#include "remoting/test/fake_network_dispatcher.h"
#include "remoting/test/fake_network_manager.h"
#include "remoting/test/fake_socket_factory.h"

namespace remoting {

namespace {

class FakePortAllocatorSession
    : public cricket::HttpPortAllocatorSessionBase {
 public:
  FakePortAllocatorSession(
      cricket::HttpPortAllocatorBase* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay);
  virtual ~FakePortAllocatorSession();

  // cricket::HttpPortAllocatorBase overrides.
  virtual void ConfigReady(cricket::PortConfiguration* config) OVERRIDE;
  virtual void SendSessionRequest(const std::string& host, int port) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePortAllocatorSession);
};

FakePortAllocatorSession::FakePortAllocatorSession(
    cricket::HttpPortAllocatorBase* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password,
    const std::vector<rtc::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay)
    : HttpPortAllocatorSessionBase(allocator,
                                   content_name,
                                   component,
                                   ice_username_fragment,
                                   ice_password,
                                   stun_hosts,
                                   relay_hosts,
                                   relay,
                                   std::string()) {
  set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
            cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
            cricket::PORTALLOCATOR_ENABLE_IPV6 |
            cricket::PORTALLOCATOR_DISABLE_STUN |
            cricket::PORTALLOCATOR_DISABLE_RELAY);
}

FakePortAllocatorSession::~FakePortAllocatorSession() {
}

void FakePortAllocatorSession::ConfigReady(
    cricket::PortConfiguration* config) {
  // Filter out non-UDP relay ports, so that we don't try using TCP.
  for (cricket::PortConfiguration::RelayList::iterator relay =
           config->relays.begin(); relay != config->relays.end(); ++relay) {
    cricket::PortList filtered_ports;
    for (cricket::PortList::iterator port =
             relay->ports.begin(); port != relay->ports.end(); ++port) {
      if (port->proto == cricket::PROTO_UDP) {
        filtered_ports.push_back(*port);
      }
    }
    relay->ports = filtered_ports;
  }
  cricket::BasicPortAllocatorSession::ConfigReady(config);
}

void FakePortAllocatorSession::SendSessionRequest(
    const std::string& host,
    int port) {
  ReceiveSessionResponse(std::string());
}

}  // namespace

// static
scoped_ptr<FakePortAllocator> FakePortAllocator::Create(
    scoped_refptr<FakeNetworkDispatcher> fake_network_dispatcher) {
  scoped_ptr<FakePacketSocketFactory> socket_factory(
      new FakePacketSocketFactory(fake_network_dispatcher.get()));
  scoped_ptr<rtc::NetworkManager> network_manager(
      new FakeNetworkManager(socket_factory->GetAddress()));

  return scoped_ptr<FakePortAllocator>(
      new FakePortAllocator(network_manager.Pass(), socket_factory.Pass()));
}

FakePortAllocator::FakePortAllocator(
    scoped_ptr<rtc::NetworkManager> network_manager,
    scoped_ptr<FakePacketSocketFactory> socket_factory)
    : HttpPortAllocatorBase(network_manager.get(),
                            socket_factory.get(),
                            std::string()),
      network_manager_(network_manager.Pass()),
      socket_factory_(socket_factory.Pass()) {}

FakePortAllocator::~FakePortAllocator() {
}

cricket::PortAllocatorSession* FakePortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new FakePortAllocatorSession(
      this, content_name, component, ice_username_fragment, ice_password,
      stun_hosts(), relay_hosts(), relay_token());
}

}  // namespace remoting
