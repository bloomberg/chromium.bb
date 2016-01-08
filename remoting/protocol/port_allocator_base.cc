// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/port_allocator_base.h"

#include <algorithm>
#include <map>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/base/escape.h"

namespace {

typedef std::map<std::string, std::string> StringMap;

// Parses the lines in the result of the HTTP request that are of the form
// 'a=b' and returns them in a map.
StringMap ParseMap(const std::string& string) {
  StringMap map;
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(string, '\r', '=', &pairs);

  for (auto& pair : pairs) {
    map[pair.first] = pair.second;
  }
  return map;
}

}  // namespace

namespace remoting {
namespace protocol {

const int PortAllocatorBase::kNumRetries = 5;

PortAllocatorBase::PortAllocatorBase(rtc::NetworkManager* network_manager,
                                     rtc::PacketSocketFactory* socket_factory)
    : BasicPortAllocator(network_manager, socket_factory) {}

PortAllocatorBase::~PortAllocatorBase() {}

void PortAllocatorBase::SetStunHosts(
    const std::vector<rtc::SocketAddress>& hosts) {
  stun_hosts_ = hosts;
}

void PortAllocatorBase::SetRelayHosts(const std::vector<std::string>& hosts) {
  relay_hosts_ = hosts;
}

void PortAllocatorBase::SetRelayToken(const std::string& relay) {
  relay_token_ = relay;
}

PortAllocatorSessionBase::PortAllocatorSessionBase(
    PortAllocatorBase* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd,
    const std::vector<rtc::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay_token)
    : BasicPortAllocatorSession(allocator,
                                content_name,
                                component,
                                ice_ufrag,
                                ice_pwd),
      relay_hosts_(relay_hosts),
      stun_hosts_(stun_hosts),
      relay_token_(relay_token),
      attempts_(0) {}

PortAllocatorSessionBase::~PortAllocatorSessionBase() {}

void PortAllocatorSessionBase::GetPortConfigurations() {
  // Creating relay sessions can take time and is done asynchronously.
  // Creating stun sessions could also take time and could be done aysnc also,
  // but for now is done here and added to the initial config.  Note any later
  // configs will have unresolved stun ips and will be discarded by the
  // AllocationSequence.
  cricket::ServerAddresses hosts;
  for (const auto& host : stun_hosts_) {
    hosts.insert(host);
  }

  cricket::PortConfiguration* config =
      new cricket::PortConfiguration(hosts, username(), password());
  ConfigReady(config);
  TryCreateRelaySession();
}

void PortAllocatorSessionBase::TryCreateRelaySession() {
  if (allocator()->flags() & cricket::PORTALLOCATOR_DISABLE_RELAY)
    return;

  if (attempts_ == PortAllocatorBase::kNumRetries) {
    LOG(ERROR) << "PortAllocator: maximum number of requests reached; "
                  << "giving up on relay.";
    return;
  }

  if (relay_hosts_.empty()) {
    LOG(ERROR) << "PortAllocator: no relay hosts configured.";
    return;
  }

  if (relay_token_.empty()){
    LOG(WARNING) << "No relay auth token found.";
    return;
  }

  // Choose the next host to try.
  std::string host = relay_hosts_[attempts_ % relay_hosts_.size()];
  attempts_++;
  SendSessionRequest(host);
}

PortAllocatorBase* PortAllocatorSessionBase::allocator() {
  return static_cast<PortAllocatorBase*>(
      BasicPortAllocatorSession::allocator());
}

std::string PortAllocatorSessionBase::GetSessionRequestUrl() {
  ASSERT(!username().empty());
  ASSERT(!password().empty());
  return "/create_session?username=" +
         net::EscapeUrlEncodedData(username(), false) + "&password=" +
         net::EscapeUrlEncodedData(password(), false);
}

void PortAllocatorSessionBase::ReceiveSessionResponse(
    const std::string& response) {
  StringMap map = ParseMap(response);

  if (!username().empty() && map["username"] != username()) {
    LOG(WARNING) << "Received unexpected username value from relay server.";
  }
  if (!password().empty() && map["password"] != password()) {
    LOG(WARNING) << "Received unexpected password value from relay server.";
  }

  cricket::ServerAddresses hosts;
  for (const auto& host : stun_hosts_) {
    hosts.insert(host);
  }

  cricket::PortConfiguration* config =
      new cricket::PortConfiguration(hosts, map["username"], map["password"]);

  std::string relay_ip = map["relay.ip"];
  std::string relay_port = map["relay.udp_port"];
  unsigned relay_port_int;

  if (!relay_ip.empty() && !relay_port.empty() &&
      base::StringToUint(relay_port, &relay_port_int)) {
    cricket::RelayServerConfig relay_config(cricket::RELAY_GTURN);
    rtc::SocketAddress address(relay_ip, relay_port_int);
    relay_config.ports.push_back(
        cricket::ProtocolAddress(address, cricket::PROTO_UDP));
    config->AddRelay(relay_config);
  }

  ConfigReady(config);
}

}  // namespace protocol
}  // namespace remoting
