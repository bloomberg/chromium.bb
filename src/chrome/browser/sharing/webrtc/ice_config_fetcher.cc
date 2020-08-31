// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/ice_config_fetcher.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {
const char kIceConfigApiUrl[] =
    "https://networktraversal.googleapis.com/v1alpha/iceconfig?key=";

// Response with 2 ice server configs takes ~1KB. A loose upper bound of 16KB is
// chosen to avoid breaking the flow in case the response has longer URLs in ice
// configs.
constexpr int kMaxBodySize = 16 * 1024;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ice_config_fetcher", R"(
        semantics {
          sender: "IceConfigFetcher"
          description:
            "Fetches ice server configurations for p2p webrtc connection as "
            "described in "
            "https://www.w3.org/TR/webrtc/#rtciceserver-dictionary."
          trigger:
            "User uses any Chrome cross-device sharing feature and selects one"
            " of their devices to send the data to."
          data: "No data is sent in the request."
          destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can disable this behavior by signing out of Chrome."
            chrome_policy {
              BrowserSignin {
                policy_options {mode: MANDATORY}
                BrowserSignin: 0
              }
            }
          })");

bool IsLoaderSuccessful(const network::SimpleURLLoader* loader) {
  if (!loader || loader->NetError() != net::OK)
    return false;

  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers)
    return false;

  // Success response codes are 2xx.
  return (loader->ResponseInfo()->headers->response_code() / 100) == 2;
}

}  // namespace

IceConfigFetcher::IceConfigFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

IceConfigFetcher::~IceConfigFetcher() = default;

void IceConfigFetcher::GetIceServers(IceServerCallback callback) {
  url_loader_.reset();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL(base::StrCat({kIceConfigApiUrl, google_apis::GetSharingAPIKey()}));
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IceConfigFetcher::OnIceServersResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      kMaxBodySize);
}

void IceConfigFetcher::OnIceServersResponse(
    IceServerCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::vector<sharing::mojom::IceServerPtr> ice_servers;

  if (IsLoaderSuccessful(url_loader_.get()) && response_body)
    ice_servers = ParseIceConfigJson(*response_body);

  sharing::LogWebRtcIceConfigFetched(ice_servers.size());

  if (ice_servers.empty())
    ice_servers = GetDefaultIceServers();

  std::move(callback).Run(std::move(ice_servers));
}

std::vector<sharing::mojom::IceServerPtr> IceConfigFetcher::ParseIceConfigJson(
    std::string json) {
  std::vector<sharing::mojom::IceServerPtr> ice_servers;
  base::Optional<base::Value> response = base::JSONReader::Read(json);
  if (!response)
    return ice_servers;

  base::Value* ice_servers_json = response->FindListKey("iceServers");
  if (!ice_servers_json)
    return ice_servers;

  for (base::Value& server : ice_servers_json->GetList()) {
    const base::Value* urls_json = server.FindListKey("urls");
    if (!urls_json)
      continue;

    std::vector<GURL> urls;
    for (const base::Value& url_json : urls_json->GetList()) {
      std::string url;
      if (!url_json.GetAsString(&url))
        continue;

      urls.emplace_back(url);
    }

    if (urls.empty())
      continue;

    sharing::mojom::IceServerPtr ice_server(sharing::mojom::IceServer::New());
    ice_server->urls = std::move(urls);

    std::string* retrieved_username = server.FindStringKey("username");
    if (retrieved_username)
      ice_server->username.emplace(std::move(*retrieved_username));

    std::string* retrieved_credential = server.FindStringKey("credential");
    if (retrieved_credential)
      ice_server->credential.emplace(std::move(*retrieved_credential));

    ice_servers.push_back(std::move(ice_server));
  }

  return ice_servers;
}

// static
std::vector<sharing::mojom::IceServerPtr>
IceConfigFetcher::GetDefaultIceServers() {
  sharing::mojom::IceServerPtr ice_server(sharing::mojom::IceServer::New());
  ice_server->urls.emplace_back("stun:stun.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun1.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun2.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun3.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun4.l.google.com:19302");

  std::vector<sharing::mojom::IceServerPtr> default_servers;
  default_servers.push_back(std::move(ice_server));
  return default_servers;
}
