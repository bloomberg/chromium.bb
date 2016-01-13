// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_port_allocator.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "remoting/client/plugin/pepper_network_manager.h"
#include "remoting/client/plugin/pepper_packet_socket_factory.h"
#include "remoting/client/plugin/pepper_util.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {

namespace {

// Read buffer we allocate per read when reading response from
// URLLoader. Normally the response from URL loader is smaller than 1kB.
const int kReadSize = 1024;

class PepperPortAllocatorSession : public protocol::PortAllocatorSessionBase {
 public:
  PepperPortAllocatorSession(
      PepperPortAllocator* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password);
  ~PepperPortAllocatorSession() override;

  // PortAllocatorBase overrides.
  void SendSessionRequest(const std::string& host) override;

 private:
  void OnUrlOpened(int32_t result);
  void ReadResponseBody();
  void OnResponseBodyRead(int32_t result);

  pp::InstanceHandle pp_instance_;

  cricket::ServerAddresses stun_hosts_;

  scoped_ptr<pp::URLLoader> relay_url_loader_;
  std::vector<char> relay_response_body_;
  bool relay_response_received_ = false;

  pp::CompletionCallbackFactory<PepperPortAllocatorSession> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocatorSession);
};

PepperPortAllocatorSession::PepperPortAllocatorSession(
    PepperPortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password)
    : PortAllocatorSessionBase(allocator,
                               content_name,
                               component,
                               ice_username_fragment,
                               ice_password),
      pp_instance_(allocator->pp_instance()),
      callback_factory_(this) {}

PepperPortAllocatorSession::~PepperPortAllocatorSession() {}

void PepperPortAllocatorSession::SendSessionRequest(const std::string& host) {
  relay_url_loader_.reset(new pp::URLLoader(pp_instance_));
  pp::URLRequestInfo request_info(pp_instance_);
  std::string url = "https://" + host + GetSessionRequestUrl() + "&sn=1";
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

PepperPortAllocator::PepperPortAllocator(
    scoped_refptr<protocol::TransportContext> transport_context,
    pp::InstanceHandle pp_instance)
    : PortAllocatorBase(
          make_scoped_ptr(new PepperNetworkManager(pp_instance)),
          make_scoped_ptr(new PepperPacketSocketFactory(pp_instance)),
          transport_context),
      pp_instance_(pp_instance) {}

PepperPortAllocator::~PepperPortAllocator() {}

cricket::PortAllocatorSession* PepperPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new PepperPortAllocatorSession(this, content_name, component,
                                        ice_username_fragment, ice_password);
}

PepperPortAllocatorFactory::PepperPortAllocatorFactory(
    pp::InstanceHandle pp_instance)
    : pp_instance_(pp_instance) {}

PepperPortAllocatorFactory::~PepperPortAllocatorFactory() {}

scoped_ptr<cricket::PortAllocator>
PepperPortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<protocol::TransportContext> transport_context) {
  return make_scoped_ptr(
      new PepperPortAllocator(transport_context, pp_instance_));
}

}  // namespace remoting
