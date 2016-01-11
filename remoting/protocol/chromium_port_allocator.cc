// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_port_allocator.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/transport_context.h"
#include "url/gurl.h"

namespace remoting {
namespace protocol {

namespace {

class ChromiumPortAllocatorSession : public PortAllocatorSessionBase,
                                     public net::URLFetcherDelegate {
 public:
  ChromiumPortAllocatorSession(ChromiumPortAllocator* allocator,
                               const std::string& content_name,
                               int component,
                               const std::string& ice_username_fragment,
                               const std::string& ice_password);
  ~ChromiumPortAllocatorSession() override;

  // PortAllocatorBase overrides.
  void SendSessionRequest(const std::string& host) override;

  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* url_fetcher) override;

 private:
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  std::set<const net::URLFetcher*> url_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocatorSession);
};

ChromiumPortAllocatorSession::ChromiumPortAllocatorSession(
    ChromiumPortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password)
    : PortAllocatorSessionBase(allocator,
                               content_name,
                               component,
                               ice_username_fragment,
                               ice_password),
      url_context_(allocator->url_context()) {}

ChromiumPortAllocatorSession::~ChromiumPortAllocatorSession() {
  STLDeleteElements(&url_fetchers_);
}

void ChromiumPortAllocatorSession::SendSessionRequest(const std::string& host) {
  GURL url("https://" + host + GetSessionRequestUrl() + "&sn=1");
  scoped_ptr<net::URLFetcher> url_fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::GET, this);
  url_fetcher->SetRequestContext(url_context_.get());
  url_fetcher->AddExtraRequestHeader("X-Talk-Google-Relay-Auth: " +
                                     relay_token());
  url_fetcher->AddExtraRequestHeader("X-Google-Relay-Auth: " + relay_token());
  url_fetcher->AddExtraRequestHeader("X-Stream-Type: chromoting");
  url_fetcher->Start();
  url_fetchers_.insert(url_fetcher.release());
}

void ChromiumPortAllocatorSession::OnURLFetchComplete(
    const net::URLFetcher* source) {
  int response_code = source->GetResponseCode();
  std::string response;
  source->GetResponseAsString(&response);

  url_fetchers_.erase(source);
  delete source;

  if (response_code != net::HTTP_OK) {
    LOG(WARNING) << "Received error when allocating relay session: "
                 << response_code;
    TryCreateRelaySession();
    return;
  }

  ReceiveSessionResponse(response);
}

}  // namespace

ChromiumPortAllocator::ChromiumPortAllocator(
    scoped_ptr<rtc::NetworkManager> network_manager,
    scoped_ptr<rtc::PacketSocketFactory> socket_factory,
    scoped_refptr<TransportContext> transport_context,
    scoped_refptr<net::URLRequestContextGetter> url_context)
    : PortAllocatorBase(std::move(network_manager),
                        std::move(socket_factory),
                        transport_context),
      url_context_(url_context) {}

ChromiumPortAllocator::~ChromiumPortAllocator() {}

cricket::PortAllocatorSession* ChromiumPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new ChromiumPortAllocatorSession(this, content_name, component,
                                          ice_username_fragment, ice_password);
}

ChromiumPortAllocatorFactory::ChromiumPortAllocatorFactory(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : url_request_context_getter_(url_request_context_getter) {}

ChromiumPortAllocatorFactory::~ChromiumPortAllocatorFactory() {}

scoped_ptr<cricket::PortAllocator>
ChromiumPortAllocatorFactory::CreatePortAllocator(
    scoped_refptr<TransportContext> transport_context) {
  scoped_ptr<rtc::NetworkManager> network_manager(
      new rtc::BasicNetworkManager());
  scoped_ptr<rtc::PacketSocketFactory> socket_factory(
      new ChromiumPacketSocketFactory());
  return make_scoped_ptr(new ChromiumPortAllocator(
      std::move(network_manager), std::move(socket_factory), transport_context,
      url_request_context_getter_));
}

}  // namespace protocol
}  // namespace remoting
