// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/context_host_resolver.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "base/time/tick_clock.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/scheme_host_port.h"

namespace net {

ContextHostResolver::ContextHostResolver(
    HostResolverManager* manager,
    std::unique_ptr<ResolveContext> resolve_context)
    : manager_(manager), resolve_context_(std::move(resolve_context)) {
  DCHECK(manager_);
  DCHECK(resolve_context_);

  manager_->RegisterResolveContext(resolve_context_.get());
}

ContextHostResolver::ContextHostResolver(
    std::unique_ptr<HostResolverManager> owned_manager,
    std::unique_ptr<ResolveContext> resolve_context)
    : ContextHostResolver(owned_manager.get(), std::move(resolve_context)) {
  owned_manager_ = std::move(owned_manager);
}

ContextHostResolver::~ContextHostResolver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (owned_manager_)
    DCHECK_EQ(owned_manager_.get(), manager_);

  // No |resolve_context_| to deregister if OnShutdown() was already called.
  if (resolve_context_)
    manager_->DeregisterResolveContext(resolve_context_.get());
}

void ContextHostResolver::OnShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(resolve_context_);
  manager_->DeregisterResolveContext(resolve_context_.get());
  resolve_context_.reset();

  DCHECK(!shutting_down_);
  shutting_down_ = true;
}

std::unique_ptr<HostResolver::ResolveHostRequest>
ContextHostResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkIsolationKey network_isolation_key,
    NetLogWithSource source_net_log,
    absl::optional<ResolveHostParameters> optional_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (shutting_down_)
    return HostResolver::CreateFailingRequest(ERR_CONTEXT_SHUT_DOWN);

  return manager_->CreateRequest(
      std::move(host), std::move(network_isolation_key),
      std::move(source_net_log), std::move(optional_parameters),
      resolve_context_.get(), resolve_context_->host_cache());
}

std::unique_ptr<HostResolver::ResolveHostRequest>
ContextHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& source_net_log,
    const absl::optional<ResolveHostParameters>& optional_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (shutting_down_)
    return HostResolver::CreateFailingRequest(ERR_CONTEXT_SHUT_DOWN);

  return manager_->CreateRequest(host, network_isolation_key, source_net_log,
                                 optional_parameters, resolve_context_.get(),
                                 resolve_context_->host_cache());
}

std::unique_ptr<HostResolver::ProbeRequest>
ContextHostResolver::CreateDohProbeRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (shutting_down_)
    return HostResolver::CreateFailingProbeRequest(ERR_CONTEXT_SHUT_DOWN);

  return manager_->CreateDohProbeRequest(resolve_context_.get());
}

std::unique_ptr<HostResolver::MdnsListener>
ContextHostResolver::CreateMdnsListener(const HostPortPair& host,
                                        DnsQueryType query_type) {
  return manager_->CreateMdnsListener(host, query_type);
}

HostCache* ContextHostResolver::GetHostCache() {
  return resolve_context_->host_cache();
}

base::Value ContextHostResolver::GetDnsConfigAsValue() const {
  return manager_->GetDnsConfigAsValue();
}

void ContextHostResolver::SetRequestContext(
    URLRequestContext* request_context) {
  DCHECK(!shutting_down_);
  DCHECK(resolve_context_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  resolve_context_->set_url_request_context(request_context);
}

HostResolverManager* ContextHostResolver::GetManagerForTesting() {
  return manager_;
}

const URLRequestContext* ContextHostResolver::GetContextForTesting() const {
  return resolve_context_ ? resolve_context_->url_request_context() : nullptr;
}

size_t ContextHostResolver::LastRestoredCacheSize() const {
  return resolve_context_->host_cache()
             ? resolve_context_->host_cache()->last_restore_size()
             : 0;
}

size_t ContextHostResolver::CacheSize() const {
  return resolve_context_->host_cache() ? resolve_context_->host_cache()->size()
                                        : 0;
}

void ContextHostResolver::SetProcParamsForTesting(
    const ProcTaskParams& proc_params) {
  manager_->set_proc_params_for_test(proc_params);
}

void ContextHostResolver::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  manager_->SetTickClockForTesting(tick_clock);
  if (resolve_context_->host_cache())
    resolve_context_->host_cache()->set_tick_clock_for_testing(tick_clock);
}

}  // namespace net
