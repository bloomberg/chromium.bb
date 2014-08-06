// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_port_allocator.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "remoting/client/plugin/pepper_network_manager.h"
#include "remoting/client/plugin/pepper_packet_socket_factory.h"
#include "remoting/client/plugin/pepper_util.h"

namespace remoting {

namespace {

// Read buffer we allocate per read when reading response from
// URLLoader. Normally the response from URL loader is smaller than 1kB.
const int kReadSize = 1024;

class PepperPortAllocatorSession
    : public cricket::HttpPortAllocatorSessionBase {
 public:
  PepperPortAllocatorSession(
      cricket::HttpPortAllocatorBase* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay_token,
      const pp::InstanceHandle& instance);
  virtual ~PepperPortAllocatorSession();

  // cricket::HttpPortAllocatorBase overrides.
  virtual void ConfigReady(cricket::PortConfiguration* config) OVERRIDE;
  virtual void GetPortConfigurations() OVERRIDE;
  virtual void SendSessionRequest(const std::string& host, int port) OVERRIDE;

 private:
  void ResolveStunServerAddress();
  void OnStunAddressResolved(int32_t result);

  void OnUrlOpened(int32_t result);
  void ReadResponseBody();
  void OnResponseBodyRead(int32_t result);

  pp::InstanceHandle instance_;

  pp::HostResolver stun_address_resolver_;
  rtc::SocketAddress stun_address_;
  int stun_port_;

  scoped_ptr<pp::URLLoader> relay_url_loader_;
  std::vector<char> relay_response_body_;
  bool relay_response_received_;

  pp::CompletionCallbackFactory<PepperPortAllocatorSession> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocatorSession);
};

PepperPortAllocatorSession::PepperPortAllocatorSession(
    cricket::HttpPortAllocatorBase* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password,
    const std::vector<rtc::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay_token,
    const pp::InstanceHandle& instance)
    : HttpPortAllocatorSessionBase(allocator,
                                   content_name,
                                   component,
                                   ice_username_fragment,
                                   ice_password,
                                   stun_hosts,
                                   relay_hosts,
                                   relay_token,
                                   std::string()),
      instance_(instance),
      stun_address_resolver_(instance_),
      stun_port_(0),
      relay_response_received_(false),
      callback_factory_(this) {
  if (stun_hosts.size() > 0) {
    stun_address_ = stun_hosts[0];
  }
}

PepperPortAllocatorSession::~PepperPortAllocatorSession() {
}

void PepperPortAllocatorSession::ConfigReady(
    cricket::PortConfiguration* config) {
  if (config->stun_address.IsUnresolved()) {
    // Make sure that the address that we pass to ConfigReady() is
    // always resolved.
    if (stun_address_.IsUnresolved()) {
      config->stun_address.Clear();
    } else {
      config->stun_address = stun_address_;
    }
  }

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

void PepperPortAllocatorSession::GetPortConfigurations() {
  // Add an empty configuration synchronously, so a local connection
  // can be started immediately.
  ConfigReady(new cricket::PortConfiguration(
      rtc::SocketAddress(), std::string(), std::string()));

  ResolveStunServerAddress();
  TryCreateRelaySession();
}

void PepperPortAllocatorSession::ResolveStunServerAddress() {
  if (stun_address_.IsNil()) {
    return;
  }

  if (!stun_address_.IsUnresolved()) {
    return;
  }

  std::string hostname = stun_address_.hostname();
  uint16 port = stun_address_.port();

  PP_HostResolver_Hint hint;
  hint.flags = 0;
  hint.family = PP_NETADDRESS_FAMILY_IPV4;
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &PepperPortAllocatorSession::OnStunAddressResolved);
  int result = stun_address_resolver_.Resolve(hostname.c_str(),
                                              port,
                                              hint,
                                              callback);
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperPortAllocatorSession::OnStunAddressResolved(int32_t result) {
  if (result < 0) {
    LOG(ERROR) << "Failed to resolve stun address "
               << stun_address_.hostname() << ": " << result;
    return;
  }

  if (!stun_address_resolver_.GetNetAddressCount()) {
    LOG(WARNING) << "Received 0 addresses for stun server "
               << stun_address_.hostname();
    return;
  }

  pp::NetAddress address = stun_address_resolver_.GetNetAddress(0);
  if (address.is_null()) {
    LOG(ERROR) << "Failed to get address for STUN server "
               << stun_address_.hostname();
    return;
  }

  PpNetAddressToSocketAddress(address, &stun_address_);
  DCHECK(!stun_address_.IsUnresolved());

  if (relay_response_received_) {
    // If we've finished reading the response, then resubmit it to
    // HttpPortAllocatorSessionBase. This is necessary because STUN
    // and Relay parameters are stored together in PortConfiguration
    // and ReceiveSessionResponse() doesn't save relay session
    // configuration for the case we resolve STUN address later. This
    // method invokes overriden ConfigReady() which then submits
    // resolved |stun_address_|.
    //
    // TODO(sergeyu): Refactor HttpPortAllocatorSessionBase to fix this.
    ReceiveSessionResponse(std::string(relay_response_body_.begin(),
                                       relay_response_body_.end()));
  } else {
    ConfigReady(new cricket::PortConfiguration(
        stun_address_, std::string(), std::string()));
  }
}

void PepperPortAllocatorSession::SendSessionRequest(
    const std::string& host,
    int port) {
  relay_url_loader_.reset(new pp::URLLoader(instance_));
  pp::URLRequestInfo request_info(instance_);
  std::string url = "https://" + host + ":" + base::IntToString(port) +
      GetSessionRequestUrl() + "&sn=1";
  request_info.SetURL(url);
  request_info.SetMethod("GET");
  std::stringstream headers;
  headers << "X-Talk-Google-Relay-Auth: " << relay_token() << "\n\r";
  headers << "X-Google-Relay-Auth: " << relay_token() << "\n\r";
  headers << "X-Stream-Type: " << "chromoting" << "\n\r";
  request_info.SetHeaders(headers.str());

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&PepperPortAllocatorSession::OnUrlOpened);
  int result = relay_url_loader_->Open(request_info, callback);
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperPortAllocatorSession::OnUrlOpened(int32_t result) {
  if (result == PP_ERROR_ABORTED) {
    return;
  }

  if (result < 0) {
    LOG(WARNING) << "URLLoader failed: " << result;
    // Retry creating session.
    TryCreateRelaySession();
    return;
  }

  pp::URLResponseInfo response = relay_url_loader_->GetResponseInfo();
  DCHECK(!response.is_null());
  if (response.GetStatusCode() != 200) {
    LOG(WARNING) << "Received HTTP status code " << response.GetStatusCode();
    // Retry creating session.
    TryCreateRelaySession();
    return;
  }

  relay_response_body_.clear();
  ReadResponseBody();
}

void PepperPortAllocatorSession::ReadResponseBody() {
  int pos = relay_response_body_.size();
  relay_response_body_.resize(pos + kReadSize);
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &PepperPortAllocatorSession::OnResponseBodyRead);
  int result = relay_url_loader_->ReadResponseBody(&relay_response_body_[pos],
                                                   kReadSize,
                                                   callback);
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperPortAllocatorSession::OnResponseBodyRead(int32_t result) {
  if (result == PP_ERROR_ABORTED) {
    return;
  }

  if (result < 0) {
    LOG(WARNING) << "Failed to read HTTP response body when "
        "creating relay session: " << result;
    // Retry creating session.
    TryCreateRelaySession();
    return;
  }

  // Resize the buffer in case we've read less than was requested.
  CHECK_LE(result, kReadSize);
  CHECK_GE(static_cast<int>(relay_response_body_.size()), kReadSize);
  relay_response_body_.resize(relay_response_body_.size() - kReadSize + result);

  if (result == 0) {
    relay_response_received_ = true;
    ReceiveSessionResponse(std::string(relay_response_body_.begin(),
                                       relay_response_body_.end()));
    return;
  }

  ReadResponseBody();
}

}  // namespace

// static
scoped_ptr<PepperPortAllocator> PepperPortAllocator::Create(
    const pp::InstanceHandle& instance) {
  scoped_ptr<rtc::NetworkManager> network_manager(
      new PepperNetworkManager(instance));
  scoped_ptr<rtc::PacketSocketFactory> socket_factory(
      new PepperPacketSocketFactory(instance));
  scoped_ptr<PepperPortAllocator> result(new PepperPortAllocator(
      instance, network_manager.Pass(), socket_factory.Pass()));
  return result.Pass();
}

PepperPortAllocator::PepperPortAllocator(
    const pp::InstanceHandle& instance,
    scoped_ptr<rtc::NetworkManager> network_manager,
    scoped_ptr<rtc::PacketSocketFactory> socket_factory)
    : HttpPortAllocatorBase(network_manager.get(),
                            socket_factory.get(),
                            std::string()),
      instance_(instance),
      network_manager_(network_manager.Pass()),
      socket_factory_(socket_factory.Pass()) {
  // TCP transport is disabled becase PseudoTCP works poorly over
  // it. ENABLE_SHARED_UFRAG flag is specified so that the same
  // username fragment is shared between all candidates for this
  // channel.
  set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
            cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG|
            cricket::PORTALLOCATOR_ENABLE_IPV6);
}

PepperPortAllocator::~PepperPortAllocator() {
}

cricket::PortAllocatorSession* PepperPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
   return new PepperPortAllocatorSession(
       this, content_name, component, ice_username_fragment, ice_password,
       stun_hosts(), relay_hosts(), relay_token(), instance_);
}

}  // namespace remoting
