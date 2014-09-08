// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_info_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "net/base/net_util.h"
#include "remoting/signaling/iq_sender.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/webrtc/base/socketaddress.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {

const int kRequestTimeoutSeconds = 5;

JingleInfoRequest::JingleInfoRequest(SignalStrategy* signal_strategy)
    : iq_sender_(signal_strategy) {
}

JingleInfoRequest::~JingleInfoRequest() {}

void JingleInfoRequest::Send(const OnJingleInfoCallback& callback) {
  on_jingle_info_cb_ = callback;
  scoped_ptr<buzz::XmlElement> iq_body(
      new buzz::XmlElement(buzz::QN_JINGLE_INFO_QUERY, true));
  request_ = iq_sender_.SendIq(
      buzz::STR_GET, buzz::STR_EMPTY, iq_body.Pass(),
      base::Bind(&JingleInfoRequest::OnResponse, base::Unretained(this)));
  if (!request_) {
    // If we failed to send IqRequest it means that SignalStrategy is
    // disconnected. Notify the caller.
    std::vector<rtc::SocketAddress> stun_hosts;
    std::vector<std::string> relay_hosts;
    std::string relay_token;
    on_jingle_info_cb_.Run(relay_token, relay_hosts, stun_hosts);
    return;
  }
  request_->SetTimeout(base::TimeDelta::FromSeconds(kRequestTimeoutSeconds));
}

void JingleInfoRequest::OnResponse(IqRequest* request,
                                   const buzz::XmlElement* stanza) {
  std::vector<rtc::SocketAddress> stun_hosts;
  std::vector<std::string> relay_hosts;
  std::string relay_token;

  if (!stanza) {
    LOG(WARNING) << "Jingle info request has timed out.";
    on_jingle_info_cb_.Run(relay_token, relay_hosts, stun_hosts);
    return;
  }

  const buzz::XmlElement* query =
      stanza->FirstNamed(buzz::QN_JINGLE_INFO_QUERY);
  if (query == NULL) {
    LOG(WARNING) << "No Jingle info found in Jingle Info query response."
                 << stanza->Str();
    on_jingle_info_cb_.Run(relay_token, relay_hosts, stun_hosts);
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

        stun_hosts.push_back(rtc::SocketAddress(host, port));
      }
    }
  }

  const buzz::XmlElement* relay = query->FirstNamed(buzz::QN_JINGLE_INFO_RELAY);
  if (relay) {
    relay_token = relay->TextNamed(buzz::QN_JINGLE_INFO_TOKEN);
    for (const buzz::XmlElement* server =
         relay->FirstNamed(buzz::QN_JINGLE_INFO_SERVER);
         server != NULL;
         server = server->NextNamed(buzz::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(buzz::QN_JINGLE_INFO_HOST);
      if (host != buzz::STR_EMPTY)
        relay_hosts.push_back(host);
    }
  }

  on_jingle_info_cb_.Run(relay_token, relay_hosts, stun_hosts);
}

}  // namespace remoting
