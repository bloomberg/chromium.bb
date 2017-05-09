// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/http_ice_config_request.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/protocol/ice_config.h"

namespace remoting {
namespace protocol {

namespace {

// Ensure ICE config is correct at least one hour after session starts.
const int kMinimumConfigLifetimeSeconds = 3600;

// See draft-petithuguenin-behave-turn-uris-01.
const int kDefaultStunTurnPort = 3478;
const int kDefaultTurnsPort = 5349;

bool ParseLifetime(const std::string& string, base::TimeDelta* result) {
  double seconds = 0;
  if (!base::EndsWith(string, "s", base::CompareCase::INSENSITIVE_ASCII) ||
      !base::StringToDouble(string.substr(0, string.size() - 1), &seconds)) {
    return false;
  }
  *result = base::TimeDelta::FromSecondsD(seconds);
  return true;
}

// Parses url in form of <stun|turn|turns>:<host>[:<port>][?transport=<udp|tcp>]
// and adds an entry to the |config|.
bool AddServerToConfig(std::string url,
                       const std::string& username,
                       const std::string& password,
                       IceConfig* config) {
  cricket::ProtocolType turn_transport_type = cricket::PROTO_LAST;

  const char kTcpTransportSuffix[] = "?transport=tcp";
  const char kUdpTransportSuffix[] = "?transport=udp";
  if (base::EndsWith(url, kTcpTransportSuffix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    turn_transport_type = cricket::PROTO_TCP;
    url.resize(url.size() - strlen(kTcpTransportSuffix));
  } else if (base::EndsWith(url, kUdpTransportSuffix,
                            base::CompareCase::INSENSITIVE_ASCII)) {
    turn_transport_type = cricket::PROTO_UDP;
    url.resize(url.size() - strlen(kUdpTransportSuffix));
  }

  size_t colon_pos = url.find(':');
  if (colon_pos == std::string::npos)
    return false;

  std::string protocol = url.substr(0, colon_pos);

  std::string host;
  int port;
  if (!net::ParseHostAndPort(url.substr(colon_pos + 1), &host, &port))
    return false;

  if (protocol == "stun") {
    if (port == -1)
      port = kDefaultStunTurnPort;
    config->stun_servers.push_back(rtc::SocketAddress(host, port));
  } else if (protocol == "turn") {
    if (port == -1)
      port = kDefaultStunTurnPort;
    if (turn_transport_type == cricket::PROTO_LAST)
      turn_transport_type = cricket::PROTO_UDP;
    config->turn_servers.push_back(cricket::RelayServerConfig(
        host, port, username, password, turn_transport_type, false));
  } else if (protocol == "turns") {
    if (port == -1)
      port = kDefaultTurnsPort;
    if (turn_transport_type == cricket::PROTO_LAST)
      turn_transport_type = cricket::PROTO_TCP;
    config->turn_servers.push_back(cricket::RelayServerConfig(
        host, port, username, password, turn_transport_type, true));
  } else {
    return false;
  }

  return true;
}

}  // namespace

HttpIceConfigRequest::HttpIceConfigRequest(
    UrlRequestFactory* url_request_factory,
    const std::string& url)
    : url_(url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("CRD_ice_config_request", R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Request is used by Chrome Remote Desktop to fetch ICE "
            "configuration which contains list of STUN & TURN servers and TURN "
            "credentials."
          trigger:
            "When a Chrome Remote Desktop session is being connected and "
            "periodically while a session is active, as necessary. Currently "
            "the API issues credentials that expire every 24 hours, so this "
            "request will only be sent again while session is active more than "
            "24 hours and it needs to renegotiate the ICE connection. The 24 "
            "hour period is controlled by the server and may change. In some "
            "cases, e.g. if direct connection is used, it will not trigger "
            "periodically.
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled by settings. You can block Chrome "
            "Remote Desktop as specified here: "
            "https://support.google.com/chrome/?p=remote_desktop"
          chrome_policy {
            RemoteAccessHostFirewallTraversal {
              policy_options {mode: MANDATORY}
              RemoteAccessHostFirewallTraversal: false
            }
          }
          policy_exception_justification:
            "Above specified policy is only applicable on the host side and "
            "doesn't have effect in Android and iOS client apps. The product "
            "is shipped separately from Chromium, except on Chrome OS."
        })");
  url_request_ = url_request_factory->CreateUrlRequest(
      UrlRequest::Type::POST, url_, traffic_annotation);
  url_request_->SetPostData("application/json", "");
}

HttpIceConfigRequest::~HttpIceConfigRequest() {}

void HttpIceConfigRequest::Send(const OnIceConfigCallback& callback) {
  DCHECK(on_ice_config_callback_.is_null());
  DCHECK(!callback.is_null());

  on_ice_config_callback_ = callback;
  url_request_->Start(
      base::Bind(&HttpIceConfigRequest::OnResponse, base::Unretained(this)));
}

void HttpIceConfigRequest::OnResponse(const UrlRequest::Result& result) {
  DCHECK(!on_ice_config_callback_.is_null());

  if (!result.success) {
    LOG(ERROR) << "Failed to fetch " << url_;
    base::ResetAndReturn(&on_ice_config_callback_).Run(IceConfig());
    return;
  }

  if (result.status != 200) {
    LOG(ERROR) << "Received status code " << result.status << " from " << url_
               << ": " << result.response_body;
    base::ResetAndReturn(&on_ice_config_callback_).Run(IceConfig());
    return;
  }

  std::unique_ptr<base::Value> json =
      base::JSONReader::Read(result.response_body);
  base::DictionaryValue* dictionary = nullptr;
  base::ListValue* ice_servers_list = nullptr;
  if (!json || !json->GetAsDictionary(&dictionary) ||
      !dictionary->GetList("iceServers", &ice_servers_list)) {
    LOG(ERROR) << "Received invalid response from " << url_ << ": "
               << result.response_body;
    base::ResetAndReturn(&on_ice_config_callback_).Run(IceConfig());
    return;
  }

  IceConfig ice_config;

  // Parse lifetimeDuration field.
  std::string lifetime_str;
  base::TimeDelta lifetime;
  if (!dictionary->GetString("lifetimeDuration", &lifetime_str) ||
      !ParseLifetime(lifetime_str, &lifetime)) {
    LOG(ERROR) << "Received invalid lifetimeDuration value: " << lifetime_str;

    // If the |lifetimeDuration| field is missing or cannot be parsed then mark
    // the config as expired so it will refreshed for the next session.
    ice_config.expiration_time = base::Time::Now();
  } else {
    ice_config.expiration_time =
        base::Time::Now() + lifetime -
        base::TimeDelta::FromSeconds(kMinimumConfigLifetimeSeconds);
  }

  // Parse iceServers list and store them in |ice_config|.
  bool errors_found = false;
  for (const auto& server : *ice_servers_list) {
    const base::DictionaryValue* server_dict;
    if (!server.GetAsDictionary(&server_dict)) {
      errors_found = true;
      continue;
    }

    const base::ListValue* urls_list = nullptr;
    if (!server_dict->GetList("urls", &urls_list)) {
      errors_found = true;
      continue;
    }

    std::string username;
    server_dict->GetString("username", &username);

    std::string password;
    server_dict->GetString("credential", &password);

    for (const auto& url : *urls_list) {
      std::string url_str;
      if (!url.GetAsString(&url_str)) {
        errors_found = true;
        continue;
      }
      if (!AddServerToConfig(url_str, username, password, &ice_config)) {
        LOG(ERROR) << "Invalid ICE server URL: " << url_str;
      }
    }
  }

  if (errors_found) {
    LOG(ERROR) << "Received ICE config from the server that contained errors: "
               << result.response_body;
  }

  // If there are no STUN or no TURN servers then mark the config as expired so
  // it will refreshed for the next session.
  if (errors_found || ice_config.stun_servers.empty() ||
      ice_config.turn_servers.empty()) {
    ice_config.expiration_time = base::Time::Now();
  }

  base::ResetAndReturn(&on_ice_config_callback_).Run(ice_config);
}

}  // namespace protocol
}  // namespace remoting
