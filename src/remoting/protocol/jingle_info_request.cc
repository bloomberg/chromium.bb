// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_info_request.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/signaling/iq_sender.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting {
namespace protocol {

const int kRequestTimeoutSeconds = 5;

// Get fresh STUN/Relay configuration every hour.
static const int kJingleInfoUpdatePeriodSeconds = 3600;

JingleInfoRequest::JingleInfoRequest(SignalStrategy* signal_strategy)
    : iq_sender_(signal_strategy) {}

JingleInfoRequest::~JingleInfoRequest() = default;

void JingleInfoRequest::Send(OnIceConfigCallback callback) {
  on_ice_config_callback_ = std::move(callback);
  std::unique_ptr<jingle_xmpp::XmlElement> iq_body(
      new jingle_xmpp::XmlElement(jingle_xmpp::QN_JINGLE_INFO_QUERY, true));
  request_ = iq_sender_.SendIq(
      jingle_xmpp::STR_GET, jingle_xmpp::STR_EMPTY, std::move(iq_body),
      base::Bind(&JingleInfoRequest::OnResponse, base::Unretained(this)));
  if (!request_) {
    // If we failed to send IqRequest it means that SignalStrategy is
    // disconnected. Notify the caller.
    IceConfig config;
    std::move(on_ice_config_callback_).Run(config);
    return;
  }
  request_->SetTimeout(base::TimeDelta::FromSeconds(kRequestTimeoutSeconds));
}

void JingleInfoRequest::OnResponse(IqRequest* request,
                                   const jingle_xmpp::XmlElement* stanza) {
  IceConfig result;

  if (!stanza) {
    LOG(WARNING) << "Jingle info request has timed out.";
    if (on_ice_config_callback_) {
      std::move(on_ice_config_callback_).Run(result);
    }
    return;
  }

  const jingle_xmpp::XmlElement* query =
      stanza->FirstNamed(jingle_xmpp::QN_JINGLE_INFO_QUERY);
  if (query == nullptr) {
    LOG(WARNING) << "No Jingle info found in Jingle Info query response."
                 << stanza->Str();
    if (on_ice_config_callback_) {
      std::move(on_ice_config_callback_).Run(result);
    }
    return;
  }

  const jingle_xmpp::XmlElement* stun = query->FirstNamed(jingle_xmpp::QN_JINGLE_INFO_STUN);
  if (stun) {
    for (const jingle_xmpp::XmlElement* server =
         stun->FirstNamed(jingle_xmpp::QN_JINGLE_INFO_SERVER);
         server != nullptr;
         server = server->NextNamed(jingle_xmpp::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(jingle_xmpp::QN_JINGLE_INFO_HOST);
      std::string port_str = server->Attr(jingle_xmpp::QN_JINGLE_INFO_UDP);
      if (host != jingle_xmpp::STR_EMPTY && port_str != jingle_xmpp::STR_EMPTY) {
        int port;
        if (!base::StringToInt(port_str, &port)) {
          LOG(WARNING) << "Unable to parse port in stanza" << stanza->Str();
          continue;
        }

        result.stun_servers.push_back(rtc::SocketAddress(host, port));
      }
    }
  }

  const jingle_xmpp::XmlElement* relay = query->FirstNamed(jingle_xmpp::QN_JINGLE_INFO_RELAY);
  if (relay) {
    result.relay_token = relay->TextNamed(jingle_xmpp::QN_JINGLE_INFO_TOKEN);
    for (const jingle_xmpp::XmlElement* server =
         relay->FirstNamed(jingle_xmpp::QN_JINGLE_INFO_SERVER);
         server != nullptr;
         server = server->NextNamed(jingle_xmpp::QN_JINGLE_INFO_SERVER)) {
      std::string host = server->Attr(jingle_xmpp::QN_JINGLE_INFO_HOST);
      if (host != jingle_xmpp::STR_EMPTY)
        result.relay_servers.push_back(host);
    }
  }

  result.expiration_time =
      base::Time::Now() +
      base::TimeDelta::FromSeconds(kJingleInfoUpdatePeriodSeconds);

  if (on_ice_config_callback_) {
    std::move(on_ice_config_callback_).Run(result);
  }
}

}  // namespace protocol
}  // namespace remoting
