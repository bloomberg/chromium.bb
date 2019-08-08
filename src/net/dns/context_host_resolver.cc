// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/context_host_resolver.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/time/tick_clock.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/url_request/url_request_context.h"

namespace net {

// Wrapper of ResolveHostRequests that on destruction will remove itself from
// |ContextHostResolver::active_requests_|.
class ContextHostResolver::WrappedRequest
    : public HostResolver::ResolveHostRequest {
 public:
  WrappedRequest(
      std::unique_ptr<HostResolverManager::CancellableRequest> inner_request,
      ContextHostResolver* resolver)
      : inner_request_(std::move(inner_request)), resolver_(resolver) {}

  ~WrappedRequest() override { Cancel(); }

  void Cancel() {
    // Cannot destroy |inner_request_| because it is still allowed to call
    // Get...Results() methods if the request was already complete.
    inner_request_->Cancel();

    if (resolver_) {
      resolver_->active_requests_.erase(this);
      resolver_ = nullptr;
    }
  }

  int Start(CompletionOnceCallback callback) override {
    DCHECK(resolver_);
    return inner_request_->Start(std::move(callback));
  }

  const base::Optional<AddressList>& GetAddressResults() const override {
    return inner_request_->GetAddressResults();
  }

  const base::Optional<std::vector<std::string>>& GetTextResults()
      const override {
    return inner_request_->GetTextResults();
  }

  const base::Optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    return inner_request_->GetHostnameResults();
  }

  const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    return inner_request_->GetStaleInfo();
  }

  void ChangeRequestPriority(RequestPriority priority) override {
    inner_request_->ChangeRequestPriority(priority);
  }

 private:
  std::unique_ptr<HostResolverManager::CancellableRequest> inner_request_;

  // Resolver is expected to call Cancel() on destruction, clearing the pointer
  // before it becomes invalid.
  ContextHostResolver* resolver_;

  DISALLOW_COPY_AND_ASSIGN(WrappedRequest);
};

ContextHostResolver::ContextHostResolver(HostResolverManager* manager,
                                         std::unique_ptr<HostCache> host_cache)
    : manager_(manager), host_cache_(std::move(host_cache)) {
  DCHECK(manager_);
}

ContextHostResolver::ContextHostResolver(
    std::unique_ptr<HostResolverManager> owned_manager,
    std::unique_ptr<HostCache> host_cache)
    : manager_(owned_manager.get()),
      owned_manager_(std::move(owned_manager)),
      host_cache_(std::move(host_cache)) {
  DCHECK(manager_);
}

ContextHostResolver::~ContextHostResolver() {
  if (owned_manager_)
    DCHECK_EQ(owned_manager_.get(), manager_);

  // Silently cancel all requests associated with this resolver.
  while (!active_requests_.empty())
    (*active_requests_.begin())->Cancel();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
ContextHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  // TODO(crbug.com/934402): DHCECK |context_| once universally set.
  auto request = std::make_unique<WrappedRequest>(
      manager_->CreateRequest(host, source_net_log, optional_parameters,
                              context_, host_cache_.get()),
      this);
  active_requests_.insert(request.get());
  return request;
}

std::unique_ptr<HostResolver::MdnsListener>
ContextHostResolver::CreateMdnsListener(const HostPortPair& host,
                                        DnsQueryType query_type) {
  return manager_->CreateMdnsListener(host, query_type);
}

void ContextHostResolver::SetDnsClientEnabled(bool enabled) {
  manager_->SetDnsClientEnabled(enabled);
}

HostCache* ContextHostResolver::GetHostCache() {
  return manager_->GetHostCache();
}

bool ContextHostResolver::HasCached(base::StringPiece hostname,
                                    HostCache::Entry::Source* source_out,
                                    HostCache::EntryStaleness* stale_out,
                                    bool* secure_out) const {
  return manager_->HasCached(hostname, source_out, stale_out, secure_out);
}

std::unique_ptr<base::Value> ContextHostResolver::GetDnsConfigAsValue() const {
  return manager_->GetDnsConfigAsValue();
}

void ContextHostResolver::SetNoIPv6OnWifi(bool no_ipv6_on_wifi) {
  manager_->SetNoIPv6OnWifi(no_ipv6_on_wifi);
}

bool ContextHostResolver::GetNoIPv6OnWifi() {
  return manager_->GetNoIPv6OnWifi();
}

void ContextHostResolver::SetDnsConfigOverrides(
    const DnsConfigOverrides& overrides) {
  manager_->SetDnsConfigOverrides(overrides);
}

void ContextHostResolver::SetRequestContext(
    URLRequestContext* request_context) {
  DCHECK(request_context);
  DCHECK(!context_);

  context_ = request_context;
}

const std::vector<DnsConfig::DnsOverHttpsServerConfig>*
ContextHostResolver::GetDnsOverHttpsServersForTesting() const {
  return manager_->GetDnsOverHttpsServersForTesting();
}

HostResolverManager* ContextHostResolver::GetManagerForTesting() {
  return manager_;
}

const URLRequestContext* ContextHostResolver::GetContextForTesting() const {
  return context_;
}

size_t ContextHostResolver::LastRestoredCacheSize() const {
  return manager_->LastRestoredCacheSize();
}

size_t ContextHostResolver::CacheSize() const {
  return manager_->CacheSize();
}

void ContextHostResolver::SetProcParamsForTesting(
    const ProcTaskParams& proc_params) {
  manager_->set_proc_params_for_test(proc_params);
}

void ContextHostResolver::SetDnsClientForTesting(
    std::unique_ptr<DnsClient> dns_client) {
  manager_->SetDnsClient(std::move(dns_client));
}

void ContextHostResolver::SetBaseDnsConfigForTesting(
    const DnsConfig& base_config) {
  manager_->SetBaseDnsConfigForTesting(base_config);
}

void ContextHostResolver::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  manager_->SetTickClockForTesting(tick_clock);
}

}  // namespace net
