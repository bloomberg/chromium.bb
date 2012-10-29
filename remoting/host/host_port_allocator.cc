// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_port_allocator.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/network_settings.h"
#include "remoting/jingle_glue/chromium_socket_factory.h"

namespace remoting {

namespace {

class HostPortAllocatorSession
    : public cricket::HttpPortAllocatorSessionBase,
      public net::URLFetcherDelegate {
 public:
  HostPortAllocatorSession(
      cricket::HttpPortAllocatorBase* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_username_fragment,
      const std::string& ice_password,
      const std::vector<talk_base::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const scoped_refptr<net::URLRequestContextGetter>& url_context);
  virtual ~HostPortAllocatorSession();

  // cricket::HttpPortAllocatorBase overrides.
  virtual void ConfigReady(cricket::PortConfiguration* config) OVERRIDE;
  virtual void SendSessionRequest(const std::string& host, int port) OVERRIDE;

  // net::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* url_fetcher) OVERRIDE;

 private:
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  std::set<const net::URLFetcher*> url_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(HostPortAllocatorSession);
};

HostPortAllocatorSession::HostPortAllocatorSession(
    cricket::HttpPortAllocatorBase* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password,
    const std::vector<talk_base::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay,
    const scoped_refptr<net::URLRequestContextGetter>& url_context)
    : HttpPortAllocatorSessionBase(
        allocator, content_name, component, ice_username_fragment, ice_password,
        stun_hosts, relay_hosts, relay, ""),
      url_context_(url_context) {
}

HostPortAllocatorSession::~HostPortAllocatorSession() {
  STLDeleteElements(&url_fetchers_);
}

void HostPortAllocatorSession::ConfigReady(cricket::PortConfiguration* config) {
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

void HostPortAllocatorSession::SendSessionRequest(const std::string& host,
                                                  int port) {
  GURL url("https://" + host + ":" + base::IntToString(port) +
           GetSessionRequestUrl() + "&sn=1");
  scoped_ptr<net::URLFetcher> url_fetcher(
      net::URLFetcher::Create(url, net::URLFetcher::GET, this));
  url_fetcher->SetRequestContext(url_context_);
  url_fetcher->AddExtraRequestHeader(
      "X-Talk-Google-Relay-Auth: " + relay_token());
  url_fetcher->AddExtraRequestHeader("X-Google-Relay-Auth: " + relay_token());
  url_fetcher->AddExtraRequestHeader("X-Stream-Type: chromoting");
  url_fetcher->Start();
  url_fetchers_.insert(url_fetcher.release());
}

void HostPortAllocatorSession::OnURLFetchComplete(
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
scoped_ptr<HostPortAllocator> HostPortAllocator::Create(
    const scoped_refptr<net::URLRequestContextGetter>& url_context,
    const NetworkSettings& network_settings) {
  scoped_ptr<talk_base::NetworkManager> network_manager(
      new talk_base::BasicNetworkManager());
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory(
      new remoting::ChromiumPacketSocketFactory());
  scoped_ptr<HostPortAllocator> result(
      new HostPortAllocator(url_context, network_manager.Pass(),
                            socket_factory.Pass()));

  // We always use PseudoTcp to provide a reliable channel. It
  // provides poor performance when combined with TCP-based transport,
  // so we have to disable TCP ports.
  // ENABLE_SHARED_UFRAG flag is
  // specified so that the same username fragment is shared between
  // all candidates for this channel.
  int flags = cricket::PORTALLOCATOR_DISABLE_TCP |
      cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG;
  if (network_settings.nat_traversal_mode !=
      NetworkSettings::NAT_TRAVERSAL_ENABLED) {
    flags |= cricket::PORTALLOCATOR_DISABLE_STUN |
        cricket::PORTALLOCATOR_DISABLE_RELAY;
  }
  result->set_flags(flags);
  result->SetPortRange(network_settings.min_port,
                       network_settings.max_port);

  return result.Pass();
}

HostPortAllocator::HostPortAllocator(
    const scoped_refptr<net::URLRequestContextGetter>& url_context,
    scoped_ptr<talk_base::NetworkManager> network_manager,
    scoped_ptr<talk_base::PacketSocketFactory> socket_factory)
    : HttpPortAllocatorBase(network_manager.get(), socket_factory.get(), ""),
      url_context_(url_context),
      network_manager_(network_manager.Pass()),
      socket_factory_(socket_factory.Pass()) {
}

HostPortAllocator::~HostPortAllocator() {
}

cricket::PortAllocatorSession* HostPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_username_fragment,
    const std::string& ice_password) {
  return new HostPortAllocatorSession(
      this, content_name, component, ice_username_fragment, ice_password,
      stun_hosts(), relay_hosts(), relay_token(), url_context_);
}

}  // namespace remoting
