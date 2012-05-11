// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_port_allocator.h"

#include "base/string_number_conversions.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "remoting/client/plugin/pepper_network_manager.h"
#include "remoting/client/plugin/pepper_packet_socket_factory.h"

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
      const std::string& channel_name,
      int component,
      const std::vector<talk_base::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const pp::InstanceHandle& instance);
  virtual ~PepperPortAllocatorSession();

  // cricket::HttpPortAllocatorBase overrides.
  virtual void ConfigReady(cricket::PortConfiguration* config) OVERRIDE;
  virtual void SendSessionRequest(const std::string& host, int port) OVERRIDE;

 private:
  // Callback handlers for pp::URLLoader.
  static void UrlLoaderOpenCallback(void* user_data, int32_t result);
  static void UrlLoaderReadCallback(void* user_data, int32_t result);

  void OnUrlOpened(int32_t result);
  void ReadResponseBody();
  void OnResponseBodyRead(int32_t result);

  pp::InstanceHandle instance_;

  scoped_ptr<pp::URLLoader> url_loader_;
  std::vector<char> body_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocatorSession);
};

PepperPortAllocatorSession::PepperPortAllocatorSession(
    cricket::HttpPortAllocatorBase* allocator,
    const std::string& channel_name,
    int component,
    const std::vector<talk_base::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay,
    const pp::InstanceHandle& instance)
    : HttpPortAllocatorSessionBase(
        allocator, channel_name, component, stun_hosts, relay_hosts, relay, ""),
      instance_(instance) {
  set_flags(cricket::PORTALLOCATOR_DISABLE_TCP);
}

PepperPortAllocatorSession::~PepperPortAllocatorSession() {
}

void PepperPortAllocatorSession::ConfigReady(
    cricket::PortConfiguration* config) {
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

void PepperPortAllocatorSession::SendSessionRequest(
    const std::string& host,
    int port) {
  url_loader_.reset(new pp::URLLoader(instance_));
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

  int result = url_loader_->Open(request_info, pp::CompletionCallback(
      &PepperPortAllocatorSession::UrlLoaderOpenCallback, this));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperPortAllocatorSession::OnUrlOpened(int32_t result) {
  if (result == PP_ERROR_ABORTED)
    return;

  if (result < 0) {
    LOG(WARNING) << "URLLoader failed: " << result;
    // Retry creating session.
    TryCreateRelaySession();
    return;
  }

  pp::URLResponseInfo response = url_loader_->GetResponseInfo();
  DCHECK(!response.is_null());
  if (response.GetStatusCode() != 200) {
    LOG(WARNING) << "Received HTTP status code " << response.GetStatusCode();
    // Retry creating session.
    TryCreateRelaySession();
    return;
  }

  body_.clear();
  ReadResponseBody();
}

void PepperPortAllocatorSession::ReadResponseBody() {
  int pos = body_.size();
  body_.resize(pos + kReadSize);
  int result = url_loader_->ReadResponseBody(
      &body_[pos], kReadSize, pp::CompletionCallback(
          &PepperPortAllocatorSession::UrlLoaderReadCallback, this));
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void PepperPortAllocatorSession::OnResponseBodyRead(int32_t result) {
  if (result == PP_ERROR_ABORTED)
    return;

  if (result < 0) {
    LOG(WARNING) << "Failed to read HTTP response body when "
        "creating relay session: " << result;
    // Retry creating session.
    TryCreateRelaySession();
    return;
  }

  // Resize the buffer in case we've read less than was requested.
  CHECK_LE(result, kReadSize);
  CHECK_GE(static_cast<int>(body_.size()), kReadSize);
  body_.resize(body_.size() - kReadSize + result);

  if (result == 0) {
    // Finished reading the response.
    ReceiveSessionResponse(std::string(body_.begin(), body_.end()));
    return;
  }

  ReadResponseBody();
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
