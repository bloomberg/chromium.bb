// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/http_ice_config_request.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/protocol/ice_config.h"

namespace remoting {
namespace protocol {

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
            "periodically."
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

  IceConfig ice_config = IceConfig::Parse(result.response_body);
  if (ice_config.is_null()) {
    LOG(ERROR) << "Received invalid response from " << url_ << ": "
               << result.response_body;
  }

  base::ResetAndReturn(&on_ice_config_callback_).Run(ice_config);
}

}  // namespace protocol
}  // namespace remoting
