// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_port_allocator.h"

#include "base/macros.h"
#include "remoting/test/fake_network_dispatcher.h"
#include "remoting/test/fake_network_manager.h"
#include "remoting/test/fake_socket_factory.h"
#include "third_party/webrtc/p2p/client/basicportallocator.h"

namespace remoting {

namespace {

class FakePortAllocatorSession : public protocol::PortAllocatorSessionBase {
 public:
  FakePortAllocatorSession(
      protocol::PortAllocatorBase* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay);
  ~FakePortAllocatorSession() override;

  // protocol::PortAllocatorBase overrides.
  void ConfigReady(cricket::PortConfiguration* config) override;
  void SendSessionRequest(const std::string& host) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePortAllocatorSession);
};

FakePortAllocatorSession::FakePortAllocatorSession(
    protocol::PortAllocatorBase* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password,
    const std::vector<rtc::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay)
    : PortAllocatorSessionBase(allocator,
                               content_name,
                               component,
                               ice_username_fragment,
                               ice_password,
                               stun_hosts,
                               relay_hosts,
                               relay) {}

FakePortAllocatorSession::~FakePortAllocatorSession() {}

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

void FakePortAllocatorSession::SendSessionRequest(const std::string& host) {
  ReceiveSessionResponse(std::string());
}

}  // namespace

FakePortAllocator::FakePortAllocator(rtc::NetworkManager* network_manager,
                                     FakePacketSocketFactory* socket_factory)
    : PortAllocatorBase(network_manager, socket_factory) {}
FakePortAllocator::~FakePortAllocator() {}

cricket::PortAllocatorSession* FakePortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new FakePortAllocatorSession(
      this, content_name, component, ice_username_fragment, ice_password,
      stun_hosts(), relay_hosts(), relay_token());
}

FakePortAllocatorFactory::FakePortAllocatorFactory(
    scoped_refptr<FakeNetworkDispatcher> fake_network_dispatcher) {
  socket_factory_.reset(
      new FakePacketSocketFactory(fake_network_dispatcher.get()));
  network_manager_.reset(new FakeNetworkManager(socket_factory_->GetAddress()));
}

FakePortAllocatorFactory::~FakePortAllocatorFactory() {}

scoped_ptr<protocol::PortAllocatorBase>
FakePortAllocatorFactory::CreatePortAllocator() {
  return make_scoped_ptr(
      new FakePortAllocator(network_manager_.get(), socket_factory_.get()));
}

}  // namespace remoting
