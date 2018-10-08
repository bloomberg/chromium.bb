// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/url_loader.h"

namespace network {
namespace {

void GetAlternativeProxy(const GURL& url,
                         const net::ProxyRetryInfoMap& proxy_retry_info,
                         net::ProxyInfo* result) {
  net::ProxyServer resolved_proxy_server = result->proxy_server();
  DCHECK(resolved_proxy_server.is_valid());

  // Right now, HTTPS proxies are assumed to support quic. If this needs to
  // change, add a setting in CustomProxyConfig to control this behavior.
  if (!resolved_proxy_server.is_https())
    return;

  net::ProxyInfo alternative_proxy_info;
  alternative_proxy_info.UseProxyServer(net::ProxyServer(
      net::ProxyServer::SCHEME_QUIC, resolved_proxy_server.host_port_pair()));
  alternative_proxy_info.DeprioritizeBadProxies(proxy_retry_info);

  if (alternative_proxy_info.is_empty())
    return;

  result->SetAlternativeProxy(alternative_proxy_info.proxy_server());
}

bool ApplyProxyConfigToProxyInfo(const net::ProxyConfig::ProxyRules& rules,
                                 const net::ProxyRetryInfoMap& proxy_retry_info,
                                 const GURL& url,
                                 net::ProxyInfo* proxy_info) {
  DCHECK(proxy_info);
  if (rules.empty())
    return false;

  rules.Apply(url, proxy_info);
  proxy_info->DeprioritizeBadProxies(proxy_retry_info);
  return !proxy_info->proxy_server().is_direct();
}

// Checks if |target_proxy| is in |proxy_list|.
bool CheckProxyList(const net::ProxyList& proxy_list,
                    const net::ProxyServer& target_proxy) {
  for (const auto& proxy : proxy_list.GetAll()) {
    if (proxy.host_port_pair().Equals(target_proxy.host_port_pair()))
      return true;
  }
  return false;
}

}  // namespace

NetworkServiceProxyDelegate::NetworkServiceProxyDelegate(
    mojom::CustomProxyConfigClientRequest config_client_request)
    : binding_(this, std::move(config_client_request)) {}

void NetworkServiceProxyDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::HttpRequestHeaders* headers) {
  if (!MayProxyURL(request->url()))
    return;

  headers->MergeFrom(proxy_config_->pre_cache_headers);

  auto* url_loader = URLLoader::ForRequest(*request);
  if (url_loader) {
    headers->MergeFrom(url_loader->custom_proxy_pre_cache_headers());
  }
}

void NetworkServiceProxyDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
  auto* url_loader = URLLoader::ForRequest(*request);
  if (IsInProxyConfig(proxy_info.proxy_server())) {
    headers->MergeFrom(proxy_config_->post_cache_headers);

    if (url_loader) {
      headers->MergeFrom(url_loader->custom_proxy_post_cache_headers());
    }
    // TODO(crbug.com/721403): This check may be incorrect if a new proxy config
    // is set between OnBeforeStartTransaction and here.
  } else if (MayProxyURL(request->url())) {
    for (const auto& kv : proxy_config_->pre_cache_headers.GetHeaderVector()) {
      headers->RemoveHeader(kv.key);
    }

    if (url_loader) {
      for (const auto& kv :
           url_loader->custom_proxy_pre_cache_headers().GetHeaderVector()) {
        headers->RemoveHeader(kv.key);
      }
    }
  }
}

NetworkServiceProxyDelegate::~NetworkServiceProxyDelegate() {}

void NetworkServiceProxyDelegate::OnResolveProxy(
    const GURL& url,
    const std::string& method,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  if (!EligibleForProxy(*result, url, method))
    return;

  net::ProxyInfo proxy_info;
  if (ApplyProxyConfigToProxyInfo(proxy_config_->rules, proxy_retry_info, url,
                                  &proxy_info)) {
    DCHECK(!proxy_info.is_empty() && !proxy_info.is_direct());
    result->OverrideProxyList(proxy_info.proxy_list());
    GetAlternativeProxy(url, proxy_retry_info, result);
  }
}

void NetworkServiceProxyDelegate::OnFallback(const net::ProxyServer& bad_proxy,
                                             int net_error) {}

void NetworkServiceProxyDelegate::OnCustomProxyConfigUpdated(
    mojom::CustomProxyConfigPtr proxy_config) {
  DCHECK(proxy_config->rules.empty() ||
         !proxy_config->rules.proxies_for_http.IsEmpty());
  proxy_config_ = std::move(proxy_config);
}

bool NetworkServiceProxyDelegate::IsInProxyConfig(
    const net::ProxyServer& proxy_server) const {
  if (!proxy_server.is_valid() || proxy_server.is_direct())
    return false;

  return CheckProxyList(proxy_config_->rules.proxies_for_http, proxy_server);
}

bool NetworkServiceProxyDelegate::MayProxyURL(const GURL& url) const {
  return url.SchemeIs(url::kHttpScheme) && !proxy_config_->rules.empty() &&
         !net::IsLocalhost(url);
}

bool NetworkServiceProxyDelegate::EligibleForProxy(
    const net::ProxyInfo& proxy_info,
    const GURL& url,
    const std::string& method) const {
  return proxy_info.is_direct() && proxy_info.proxy_list().size() == 1 &&
         MayProxyURL(url) && net::HttpUtil::IsMethodIdempotent(method);
}

}  // namespace network
