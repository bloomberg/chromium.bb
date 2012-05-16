// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_port_allocator.h"

#include "base/string_number_conversions.h"
#include "net/base/net_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/private/host_resolver_private.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "remoting/client/plugin/pepper_network_manager.h"
#include "remoting/client/plugin/pepper_packet_socket_factory.h"
#include "remoting/client/plugin/pepper_util.h"

namespace remoting {

namespace {

// URL used to create a relay session.
const char kCreateRelaySessionURL[] = "/create_session";

// Read buffer we allocate per read when reading response from
// URLLoader. Normally the response from URL loader is smaller than 1kB.
const int kReadSize = 1024;

class PepperPortAllocatorSession
    : public cricket::HttpPortAllocatorSessionBase {
 public:
  PepperPortAllocatorSession(
      cricket::HttpPortAllocatorBase* allocator,
      const std::string& channel_name,
      int component,
      const std::vector<talk_base::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay_token,
      const pp::InstanceHandle& instance);
  virtual ~PepperPortAllocatorSession();

  // cricket::HttpPortAllocatorBase overrides.
  virtual void ConfigReady(cricket::PortConfiguration* config) OVERRIDE;
  virtual void GetPortConfigurations() OVERRIDE;
  virtual void SendSessionRequest(const std::string& host, int port) OVERRIDE;

 private:
  // Callback handlers for Pepper APIs.
  static void HostResolverCallback(void* user_data, int32_t result);
  static void UrlLoaderOpenCallback(void* user_data, int32_t result);
  static void UrlLoaderReadCallback(void* user_data, int32_t result);

  void ResolveStunServerAddress();
  void OnStunAddressResolved(int32_t result);

  void OnUrlOpened(int32_t result);
  void ReadResponseBody();
  void OnResponseBodyRead(int32_t result);

  pp::InstanceHandle instance_;

  pp::HostResolverPrivate stun_address_resolver_;
  talk_base::SocketAddress stun_address_;
  int stun_port_;

  scoped_ptr<pp::URLLoader> relay_url_loader_;
  std::vector<char> relay_response_body_;
  bool relay_response_received_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocatorSession);
};

PepperPortAllocatorSession::PepperPortAllocatorSession(
    cricket::HttpPortAllocatorBase* allocator,
    const std::string& channel_name,
    int component,
    const std::vector<talk_base::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay_token,
    const pp::InstanceHandle& instance)
    : cricket::HttpPortAllocatorSessionBase(
        allocator, channel_name, component, stun_hosts,
        relay_hosts, relay_token, ""),
      instance_(instance),
      stun_address_resolver_(instance_),
      stun_port_(0),
      relay_response_received_(false) {
  set_flags(cricket::PORTALLOCATOR_DISABLE_TCP);
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
    cricket::PortConfiguration::PortList filtered_ports;
    for (cricket::PortConfiguration::PortList::iterator port =
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
  ConfigReady(new cricket::PortConfiguration(talk_base::SocketAddress()));

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

  PP_HostResolver_Private_Hint hint;
  hint.flags = 0;
  hint.family = PP_NETADDRESSFAMILY_IPV4;
  int result = stun_address_resolver_.Resolve(
      hostname, port, hint, pp::CompletionCallback(
          &PepperPortAllocatorSession::HostResolverCallback, this));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperPortAllocatorSession::OnStunAddressResolved(int32_t result) {
  if (result < 0) {
    LOG(ERROR) << "Failed to resolve stun address "
               << stun_address_.hostname() << ": " << result;
    return;
  }

  if (!stun_address_resolver_.GetSize()) {
    LOG(WARNING) << "Received 0 addresses for stun server "
               << stun_address_.hostname();
    return;
  }

  PP_NetAddress_Private address;
  if (!stun_address_resolver_.GetNetAddress(0, &address) ||
      !PpAddressToSocketAddress(address, &stun_address_)) {
    LOG(ERROR) << "Failed to get address for STUN server "
               << stun_address_.hostname();
    return;
  }

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
    ConfigReady(new cricket::PortConfiguration(stun_address_));
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
  headers << "X-StreamType: " << channel_name() << "\n\r";
  request_info.SetHeaders(headers.str());

  int result = relay_url_loader_->Open(request_info, pp::CompletionCallback(
      &PepperPortAllocatorSession::UrlLoaderOpenCallback, this));
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
  int result = relay_url_loader_->ReadResponseBody(
      &relay_response_body_[pos], kReadSize, pp::CompletionCallback(
          &PepperPortAllocatorSession::UrlLoaderReadCallback, this));
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

// static
void PepperPortAllocatorSession::HostResolverCallback(
    void* user_data,
    int32_t result) {
  PepperPortAllocatorSession* object =
      static_cast<PepperPortAllocatorSession*>(user_data);
  object->OnStunAddressResolved(result);
}

// static
void PepperPortAllocatorSession::UrlLoaderOpenCallback(
    void* user_data,
    int32_t result) {
  PepperPortAllocatorSession* object =
      static_cast<PepperPortAllocatorSession*>(user_data);
  object->OnUrlOpened(result);
}

// static
void PepperPortAllocatorSession::UrlLoaderReadCallback(
    void* user_data,
    int32_t result) {
  PepperPortAllocatorSession* object =
      static_cast<PepperPortAllocatorSession*>(user_data);
  object->OnResponseBodyRead(result);
}

}  // namespace

// static
scoped_ptr<PepperPortAllocator> PepperPortAllocator::Create(
    const pp::InstanceHandle& instance) {
  scoped_ptr<talk_base::NetworkManager> network_manager(
      new PepperNetworkManager(instance));
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory(
      new PepperPacketSocketFactory(instance));
  scoped_ptr<PepperPortAllocator> result(new PepperPortAllocator(
      instance, network_manager.Pass(), socket_factory.Pass()));
  return result.Pass();
}

PepperPortAllocator::PepperPortAllocator(
    const pp::InstanceHandle& instance,
    scoped_ptr<talk_base::NetworkManager> network_manager,
    scoped_ptr<talk_base::PacketSocketFactory> socket_factory)
    : HttpPortAllocatorBase(network_manager.get(), socket_factory.get(), ""),
      instance_(instance),
      network_manager_(network_manager.Pass()),
      socket_factory_(socket_factory.Pass()) {
}

PepperPortAllocator::~PepperPortAllocator() {
}

cricket::PortAllocatorSession* PepperPortAllocator::CreateSession(
    const std::string& channel_name,
    int component) {
  return new PepperPortAllocatorSession(
      this, channel_name, component, stun_hosts(),
      relay_hosts(), relay_token(), instance_);
}

}  // namespace remoting
