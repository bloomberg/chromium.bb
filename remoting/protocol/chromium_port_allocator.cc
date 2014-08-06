// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_port_allocator.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_socket_factory.h"
#include "remoting/protocol/network_settings.h"
#include "url/gurl.h"

namespace remoting {
namespace protocol {

namespace {

class ChromiumPortAllocatorSession
    : public cricket::HttpPortAllocatorSessionBase,
      public net::URLFetcherDelegate {
 public:
  ChromiumPortAllocatorSession(
      cricket::HttpPortAllocatorBase* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const scoped_refptr<net::URLRequestContextGetter>& url_context);
  virtual ~ChromiumPortAllocatorSession();

  // cricket::HttpPortAllocatorBase overrides.
  virtual void ConfigReady(cricket::PortConfiguration* config) OVERRIDE;
  virtual void SendSessionRequest(const std::string& host, int port) OVERRIDE;

  // net::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* url_fetcher) OVERRIDE;

 private:
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  std::set<const net::URLFetcher*> url_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumPortAllocatorSession);
};

ChromiumPortAllocatorSession::ChromiumPortAllocatorSession(
    cricket::HttpPortAllocatorBase* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password,
    const std::vector<rtc::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay,
    const scoped_refptr<net::URLRequestContextGetter>& url_context)
    : HttpPortAllocatorSessionBase(allocator,
                                   content_name,
                                   component,
                                   ice_username_fragment,
                                   ice_password,
                                   stun_hosts,
                                   relay_hosts,
                                   relay,
                                   std::string()),
      url_context_(url_context) {}

ChromiumPortAllocatorSession::~ChromiumPortAllocatorSession() {
  STLDeleteElements(&url_fetchers_);
}

void ChromiumPortAllocatorSession::ConfigReady(
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

void ChromiumPortAllocatorSession::SendSessionRequest(
    const std::string& host,
    int port) {
  GURL url("https://" + host + ":" + base::IntToString(port) +
           GetSessionRequestUrl() + "&sn=1");
  scoped_ptr<net::URLFetcher> url_fetcher(
      net::URLFetcher::Create(url, net::URLFetcher::GET, this));
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

// static
scoped_ptr<ChromiumPortAllocator> ChromiumPortAllocator::Create(
    const scoped_refptr<net::URLRequestContextGetter>& url_context,
    const NetworkSettings& network_settings) {
  scoped_ptr<rtc::NetworkManager> network_manager(
      new rtc::BasicNetworkManager());
  scoped_ptr<rtc::PacketSocketFactory> socket_factory(
      new ChromiumPacketSocketFactory());
  scoped_ptr<ChromiumPortAllocator> result(
      new ChromiumPortAllocator(url_context, network_manager.Pass(),
                            socket_factory.Pass()));

  // We always use PseudoTcp to provide a reliable channel. It provides poor
  // performance when combined with TCP-based transport, so we have to disable
  // TCP ports. ENABLE_SHARED_UFRAG flag is specified so that the same username
  // fragment is shared between all candidates for this channel.
  int flags = cricket::PORTALLOCATOR_DISABLE_TCP |
              cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
              cricket::PORTALLOCATOR_ENABLE_IPV6;

  if (!(network_settings.flags & NetworkSettings::NAT_TRAVERSAL_STUN))
    flags |= cricket::PORTALLOCATOR_DISABLE_STUN;

  if (!(network_settings.flags & NetworkSettings::NAT_TRAVERSAL_RELAY))
    flags |= cricket::PORTALLOCATOR_DISABLE_RELAY;

  result->set_flags(flags);
  result->SetPortRange(network_settings.min_port,
                       network_settings.max_port);

  return result.Pass();
}

ChromiumPortAllocator::ChromiumPortAllocator(
    const scoped_refptr<net::URLRequestContextGetter>& url_context,
    scoped_ptr<rtc::NetworkManager> network_manager,
    scoped_ptr<rtc::PacketSocketFactory> socket_factory)
    : HttpPortAllocatorBase(network_manager.get(),
                            socket_factory.get(),
                            std::string()),
      url_context_(url_context),
      network_manager_(network_manager.Pass()),
      socket_factory_(socket_factory.Pass()) {}

ChromiumPortAllocator::~ChromiumPortAllocator() {
}

cricket::PortAllocatorSession* ChromiumPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new ChromiumPortAllocatorSession(
      this, content_name, component, ice_username_fragment, ice_password,
      stun_hosts(), relay_hosts(), relay_token(), url_context_);
}

}  // namespace protocol
}  // namespace remoting
