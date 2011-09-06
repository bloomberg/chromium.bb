// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_info_request.h"

#include "base/bind.h"
#include "base/task.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "net/base/net_util.h"
#include "remoting/jingle_glue/host_resolver.h"
#include "remoting/jingle_glue/iq_request.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {


JingleInfoRequest::JingleInfoRequest(IqRequest* request,
                                     HostResolverFactory* host_resolver_factory)
    : host_resolver_factory_(host_resolver_factory),
      request_(request) {
  request_->set_callback(base::Bind(&JingleInfoRequest::OnResponse,
                                    base::Unretained(this)));
}

JingleInfoRequest::~JingleInfoRequest() {
  STLDeleteContainerPointers(stun_dns_requests_.begin(),
                             stun_dns_requests_.end());
}

void JingleInfoRequest::Send(const OnJingleInfoCallback& callback) {
  on_jingle_info_cb_ = callback;
  request_->SendIq(IqRequest::MakeIqStanza(
      buzz::STR_GET, buzz::STR_EMPTY,
      new buzz::XmlElement(buzz::QN_JINGLE_INFO_QUERY, true)));
}

void JingleInfoRequest::OnResponse(const buzz::XmlElement* stanza) {
  const buzz::XmlElement* query =
      stanza->FirstNamed(buzz::QN_JINGLE_INFO_QUERY);
  if (query == NULL) {
    LOG(WARNING) << "No Jingle info found in Jingle Info query response."
                 << stanza->Str();
    return;
  }

  const buzz::XmlElement* stun = query->FirstNamed(buzz::QN_JINGLE_INFO_STUN);
  if (stun) {
    for (const buzz::XmlElement* server =
         stun->FirstNamed(buzz::QN_JINGLE_INFO_SERVER);
         server != NULL;
         server = server->NextNamed(buzz::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(buzz::QN_JINGLE_INFO_HOST);
      std::string port_str = server->Attr(buzz::QN_JINGLE_INFO_UDP);
      if (host != buzz::STR_EMPTY && port_str != buzz::STR_EMPTY) {
        int port;
        if (!base::StringToInt(port_str, &port)) {
          LOG(WARNING) << "Unable to parse port in stanza" << stanza->Str();
          continue;
        }

        if (host_resolver_factory_) {
          net::IPAddressNumber ip_number;
          HostResolver* resolver = host_resolver_factory_->CreateHostResolver();
          stun_dns_requests_.insert(resolver);
          resolver->SignalDone.connect(
              this, &JingleInfoRequest::OnStunAddressResponse);
          resolver->Resolve(talk_base::SocketAddress(host, port));
        } else {
          // If there is no |host_resolver_factory_|, we're not sandboxed, so
          // we can let libjingle itself do the DNS resolution.
          stun_hosts_.push_back(talk_base::SocketAddress(host, port));
        }
      }
    }
  }

  const buzz::XmlElement* relay = query->FirstNamed(buzz::QN_JINGLE_INFO_RELAY);
  if (relay) {
    relay_token_ = relay->TextNamed(buzz::QN_JINGLE_INFO_TOKEN);
    for (const buzz::XmlElement* server =
         relay->FirstNamed(buzz::QN_JINGLE_INFO_SERVER);
         server != NULL;
         server = server->NextNamed(buzz::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(buzz::QN_JINGLE_INFO_HOST);
      if (host != buzz::STR_EMPTY)
        relay_hosts_.push_back(host);
    }
  }

  if (stun_dns_requests_.empty())
    on_jingle_info_cb_.Run(relay_token_, relay_hosts_, stun_hosts_);
}

void JingleInfoRequest::OnStunAddressResponse(
    HostResolver* resolver, const talk_base::SocketAddress& address) {
  if (!address.IsNil())
    stun_hosts_.push_back(address);

  MessageLoop::current()->DeleteSoon(FROM_HERE, resolver);
  stun_dns_requests_.erase(resolver);

  if (stun_dns_requests_.empty())
    on_jingle_info_cb_.Run(relay_token_, relay_hosts_, stun_hosts_);
}

}  // namespace remoting
