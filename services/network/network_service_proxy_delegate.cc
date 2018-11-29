// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/url_loader.h"

namespace network {
namespace {

// The maximum size of the cache that contains the GURLs that should use
// alternate proxy list.
constexpr size_t kMaxCacheSize = 15;

// The maximum number of previous configs to keep.
constexpr size_t kMaxPreviousConfigs = 2;

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
    if (!proxy.is_direct() &&
        proxy.host_port_pair().Equals(target_proxy.host_port_pair())) {
      return true;
    }
  }
  return false;
}

// Whether the custom proxy can proxy |url|.
bool IsURLValidForProxy(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) && !net::IsLocalhost(url);
}

// Merges headers from |in| to |out|. If the header already exists in |out| they
// are combined.
void MergeRequestHeaders(net::HttpRequestHeaders* out,
                         const net::HttpRequestHeaders& in) {
  for (net::HttpRequestHeaders::Iterator it(in); it.GetNext();) {
    std::string old_value;
    if (out->GetHeader(it.name(), &old_value)) {
      out->SetHeader(it.name(), old_value + ", " + it.value());
    } else {
      out->SetHeader(it.name(), it.value());
    }
  }
}

}  // namespace

NetworkServiceProxyDelegate::NetworkServiceProxyDelegate(
    mojom::CustomProxyConfigPtr initial_config,
    mojom::CustomProxyConfigClientRequest config_client_request)
    : proxy_config_(std::move(initial_config)),
      binding_(this, std::move(config_client_request)),
      should_use_alternate_proxy_list_cache_(kMaxCacheSize) {
  // Make sure there is always a valid proxy config so we don't need to null
  // check it.
  if (!proxy_config_)
    proxy_config_ = mojom::CustomProxyConfig::New();
}

void NetworkServiceProxyDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::HttpRequestHeaders* headers) {
  if (!MayProxyURL(request->url()))
    return;

  MergeRequestHeaders(headers, proxy_config_->pre_cache_headers);

  auto* url_loader = URLLoader::ForRequest(*request);
  if (url_loader) {
    if (url_loader->custom_proxy_use_alternate_proxy_list()) {
      should_use_alternate_proxy_list_cache_.Put(request->url().spec(), true);
    }
    MergeRequestHeaders(headers, url_loader->custom_proxy_pre_cache_headers());
  }
}

void NetworkServiceProxyDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
  auto* url_loader = URLLoader::ForRequest(*request);
  if (IsInProxyConfig(proxy_info.proxy_server())) {
    MergeRequestHeaders(headers, proxy_config_->post_cache_headers);

    if (url_loader) {
      MergeRequestHeaders(headers,
                          url_loader->custom_proxy_post_cache_headers());
    }
  } else if (MayHaveProxiedURL(request->url())) {
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
  if (ApplyProxyConfigToProxyInfo(GetProxyRulesForURL(url), proxy_retry_info,
                                  url, &proxy_info)) {
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
  if (proxy_config_) {
    previous_proxy_configs_.push_front(std::move(proxy_config_));
    if (previous_proxy_configs_.size() > kMaxPreviousConfigs)
      previous_proxy_configs_.pop_back();
  }
  proxy_config_ = std::move(proxy_config);
}

void NetworkServiceProxyDelegate::MarkProxiesAsBad(
    base::TimeDelta bypass_duration,
    const net::ProxyList& bad_proxies_list,
    MarkProxiesAsBadCallback callback) {
  std::vector<net::ProxyServer> bad_proxies = bad_proxies_list.GetAll();

  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  //
  // TODO(eroman): Support this more directly on ProxyResolutionService.
  net::ProxyList proxy_list;
  for (const auto& bad_proxy : bad_proxies)
    proxy_list.AddProxyServer(bad_proxy);
  proxy_list.AddProxyServer(net::ProxyServer::Direct());

  net::ProxyInfo proxy_info;
  proxy_info.UseProxyList(proxy_list);

  proxy_resolution_service_->MarkProxiesAsBadUntil(
      proxy_info, bypass_duration, bad_proxies, net::NetLogWithSource());

  std::move(callback).Run();
}

void NetworkServiceProxyDelegate::ClearBadProxiesCache() {
  proxy_resolution_service_->ClearBadProxiesCache();
}

bool NetworkServiceProxyDelegate::IsInProxyConfig(
    const net::ProxyServer& proxy_server) const {
  if (!proxy_server.is_valid() || proxy_server.is_direct())
    return false;

  if (CheckProxyList(proxy_config_->rules.proxies_for_http, proxy_server))
    return true;

  for (const auto& config : previous_proxy_configs_) {
    if (CheckProxyList(config->rules.proxies_for_http, proxy_server))
      return true;
  }

  return false;
}

bool NetworkServiceProxyDelegate::MayProxyURL(const GURL& url) const {
  return IsURLValidForProxy(url) && !proxy_config_->rules.empty();
}

bool NetworkServiceProxyDelegate::MayHaveProxiedURL(const GURL& url) const {
  if (!IsURLValidForProxy(url))
    return false;

  if (!proxy_config_->rules.empty())
    return true;

  for (const auto& config : previous_proxy_configs_) {
    if (!config->rules.empty())
      return true;
  }

  return false;
}

bool NetworkServiceProxyDelegate::EligibleForProxy(
    const net::ProxyInfo& proxy_info,
    const GURL& url,
    const std::string& method) const {
  return proxy_info.is_direct() && proxy_info.proxy_list().size() == 1 &&
         MayProxyURL(url) && net::HttpUtil::IsMethodIdempotent(method);
}

net::ProxyConfig::ProxyRules NetworkServiceProxyDelegate::GetProxyRulesForURL(
    const GURL& url) const {
  net::ProxyConfig::ProxyRules rules = proxy_config_->rules;
  const auto iter = should_use_alternate_proxy_list_cache_.Peek(url.spec());
  if (iter == should_use_alternate_proxy_list_cache_.end())
    return rules;

  rules.proxies_for_http = proxy_config_->alternate_proxy_list;
  return rules;
}

}  // namespace network
