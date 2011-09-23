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
#include "remoting/jingle_glue/iq_request.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

namespace remoting {


JingleInfoRequest::JingleInfoRequest(IqRequest* request)
    : request_(request) {
  request_->set_callback(base::Bind(&JingleInfoRequest::OnResponse,
                                    base::Unretained(this)));
}

JingleInfoRequest::~JingleInfoRequest() {
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

  std::vector<talk_base::SocketAddress> stun_hosts;
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

        stun_hosts.push_back(talk_base::SocketAddress(host, port));
      }
    }
  }

  std::vector<std::string> relay_hosts;
  std::string relay_token;
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
