// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/remoting_ice_config_request.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/network_traversal_messages.pb.h"
#include "remoting/protocol/ice_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {
namespace protocol {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remoting_ice_config_request",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Request used by Chrome Remote Desktop to fetch ICE (Interactive "
            "Connectivity Establishment) configuration, which contains list of "
            "STUN (Session Traversal Utilities for NAT) & TURN (Traversal "
            "Using Relay NAT) servers and TURN credentials. Please refer to "
            "https://tools.ietf.org/html/rfc5245 for more details."
          trigger:
            "When a Chrome Remote Desktop session is being connected and "
            "periodically (less frequent than once per hour) while a session "
            "is active, if the configuration is expired."
          data:
            "None (anonymous request)."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");

constexpr char kGetIceConfigPath[] = "/v1/networktraversal:geticeconfig";

}  // namespace

RemotingIceConfigRequest::RemotingIceConfigRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(ServiceUrls::GetInstance()->remoting_server_endpoint(),
                   nullptr,
                   url_loader_factory) {}

RemotingIceConfigRequest::~RemotingIceConfigRequest() = default;

void RemotingIceConfigRequest::Send(OnIceConfigCallback callback) {
  DCHECK(on_ice_config_callback_.is_null());
  DCHECK(!callback.is_null());

  on_ice_config_callback_ = std::move(callback);

  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->authenticated = false;
  request_config->path = kGetIceConfigPath;
  request_config->request_message =
      std::make_unique<apis::v1::GetIceConfigRequest>();
#if defined(OS_CHROMEOS)
  // Use the default Chrome API key for ChromeOS as the only host instance
  // which runs there is used for the ChromeOS Enterprise Kiosk mode
  // scenario.  If we decide to implement a remote access host for ChromeOS,
  // then we will need a way for the caller to provide an API key.
  request_config->api_key = google_apis::GetAPIKey();
#else
  request_config->api_key = google_apis::GetRemotingAPIKey();
#endif
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(base::BindOnce(
      &RemotingIceConfigRequest::OnResponse, base::Unretained(this)));
  http_client_.ExecuteRequest(std::move(request));
}

void RemotingIceConfigRequest::OnResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::GetIceConfigResponse> response) {
  DCHECK(!on_ice_config_callback_.is_null());

  if (!status.ok()) {
    LOG(ERROR) << "Received error code: "
               << static_cast<int>(status.error_code())
               << ", message: " << status.error_message();
    std::move(on_ice_config_callback_).Run(IceConfig());
    return;
  }

  std::move(on_ice_config_callback_).Run(IceConfig::Parse(*response));
}

}  // namespace protocol
}  // namespace remoting
