// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_host_resolver.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_results.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

// Cache size for the MockCachingHostResolver.
const unsigned kMaxCacheEntries = 100;
// TTL for the successful resolutions. Failures are not cached.
const unsigned kCacheEntryTTLSeconds = 60;

base::StringPiece GetScheme(
    const absl::variant<url::SchemeHostPort, HostPortPair>& endpoint) {
  DCHECK(absl::holds_alternative<url::SchemeHostPort>(endpoint));
  return absl::get<url::SchemeHostPort>(endpoint).scheme();
}

// In HostPortPair format (no brackets around IPv6 literals) purely for
// compatibility with IPAddress::AssignFromIPLiteral().
base::StringPiece GetHostname(
    const absl::variant<url::SchemeHostPort, HostPortPair>& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    base::StringPiece hostname =
        absl::get<url::SchemeHostPort>(endpoint).host();
    if (hostname.size() >= 2 && hostname.front() == '[' &&
        hostname.back() == ']') {
      return hostname.substr(1, hostname.size() - 2);
    }
    return hostname;
  }

  DCHECK(absl::holds_alternative<HostPortPair>(endpoint));
  return absl::get<HostPortPair>(endpoint).host();
}

uint16_t GetPort(
    const absl::variant<url::SchemeHostPort, HostPortPair>& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint))
    return absl::get<url::SchemeHostPort>(endpoint).port();

  DCHECK(absl::holds_alternative<HostPortPair>(endpoint));
  return absl::get<HostPortPair>(endpoint).port();
}

absl::variant<url::SchemeHostPort, std::string> GetCacheHost(
    const absl::variant<url::SchemeHostPort, HostPortPair>& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint))
    return absl::get<url::SchemeHostPort>(endpoint);

  DCHECK(absl::holds_alternative<HostPortPair>(endpoint));
  return absl::get<HostPortPair>(endpoint).host();
}

}  // namespace

int ParseAddressList(base::StringPiece host_list,
                     const std::vector<std::string>& dns_aliases,
                     AddressList* addrlist) {
  *addrlist = AddressList();
  addrlist->SetDnsAliases(dns_aliases);
  for (const base::StringPiece& address : base::SplitStringPiece(
           host_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(address)) {
      LOG(WARNING) << "Not a supported IP literal: " << address;
      return ERR_UNEXPECTED;
    }
    addrlist->push_back(IPEndPoint(ip_address, 0));
  }
  return OK;
}

class MockHostResolverBase::RequestImpl
    : public HostResolver::ResolveHostRequest {
 public:
  RequestImpl(absl::variant<url::SchemeHostPort, HostPortPair> request_endpoint,
              const NetworkIsolationKey& network_isolation_key,
              const absl::optional<ResolveHostParameters>& optional_parameters,
              base::WeakPtr<MockHostResolverBase> resolver)
      : request_endpoint_(std::move(request_endpoint)),
        network_isolation_key_(network_isolation_key),
        parameters_(optional_parameters ? optional_parameters.value()
                                        : ResolveHostParameters()),
        priority_(parameters_.initial_priority),
        host_resolver_flags_(ParametersToHostResolverFlags(parameters_)),
        resolve_error_info_(ResolveErrorInfo(ERR_IO_PENDING)),
        id_(0),
        resolver_(resolver),
        complete_(false) {}

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override {
    if (id_ > 0) {
      if (resolver_)
        resolver_->DetachRequest(id_);
      id_ = 0;
      resolver_ = nullptr;
    }
  }

  void DetachFromResolver() {
    id_ = 0;
    resolver_ = nullptr;
  }

  int Start(CompletionOnceCallback callback) override {
    DCHECK(callback);
    // Start() may only be called once per request.
    DCHECK_EQ(0u, id_);
    DCHECK(!complete_);
    DCHECK(!callback_);
    // Parent HostResolver must still be alive to call Start().
    DCHECK(resolver_);

    int rv = resolver_->Resolve(this);
    DCHECK(!complete_);
    if (rv == ERR_IO_PENDING) {
      DCHECK_GT(id_, 0u);
      callback_ = std::move(callback);
    } else {
      DCHECK_EQ(0u, id_);
      complete_ = true;
    }

    return rv;
  }

  const AddressList* GetAddressResults() const override {
    DCHECK(complete_);
    return base::OptionalOrNullptr(address_results_);
  }

  const std::vector<HostResolverEndpointResult>* GetEndpointResults()
      const override {
    DCHECK(complete_);
    return base::OptionalOrNullptr(endpoint_results_);
  }

  const absl::optional<std::vector<std::string>>& GetTextResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<absl::optional<std::vector<std::string>>>
        nullopt_result;
    return *nullopt_result;
  }

  const absl::optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<absl::optional<std::vector<HostPortPair>>>
        nullopt_result;
    return *nullopt_result;
  }

  const std::set<std::string>* GetDnsAliasResults() const override {
    DCHECK(complete_);
    return base::OptionalOrNullptr(fixed_up_dns_alias_results_);
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    DCHECK(complete_);
    return resolve_error_info_;
  }

  const absl::optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    DCHECK(complete_);
    return staleness_;
  }

  void ChangeRequestPriority(RequestPriority priority) override {
    priority_ = priority;
  }

  void SetError(int error) {
    // Should only be called before request is marked completed.
    DCHECK(!complete_);
    resolve_error_info_ = ResolveErrorInfo(error);
  }

  // Sets `address_results_` to `address_results`, after fixing them up.  Also
  // sets `error` to OK if the fixed up AddressList is non-empty, or
  // ERR_NAME_NOT_RESOLVED otherwise.
  void SetAddressResults(const AddressList& address_results,
                         absl::optional<HostCache::EntryStaleness> staleness) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!address_results_);
    DCHECK(!endpoint_results_);
    DCHECK(!parameters_.is_speculative);

    address_results_ = FixupAddressList(address_results);

    // If there are no addresses, either as a result of FixupAddressList(), as
    // in the originally passed in value, clear results and set an error.
    if (address_results_->empty()) {
      address_results_.reset();
      SetError(ERR_NAME_NOT_RESOLVED);
      return;
    }

    SetError(OK);

    fixed_up_dns_alias_results_ = dns_alias_utility::FixUpDnsAliases(
        std::set<std::string>(address_results_->dns_aliases().begin(),
                              address_results_->dns_aliases().end()));
    staleness_ = std::move(staleness);

    endpoint_results_ = AddressListToEndpointResults(address_results_.value());
  }

  void SetEndpointResults(
      std::vector<HostResolverEndpointResult> endpoint_results) {
    DCHECK(!complete_);
    DCHECK(!endpoint_results_);
    DCHECK(!parameters_.is_speculative);

    // TODO(crbug.com/1264933): Perform fixups on `endpoint_results`?
    endpoint_results_ = std::move(endpoint_results);

    // For now, we do not support configuring DNS aliases with endpoint results,
    // but the value is expected to always be present.
    //
    // TODO(crbug.com/1264933): Add some way to configure this, to support code
    // migrating to `HostResolverEndpointResult`.
    fixed_up_dns_alias_results_.emplace();

    // `HostResolver` implementations are expected to provide an `AddressList`
    // result whenever `HostResolverEndpointResult` is also available.
    address_results_ = EndpointResultToAddressList(
        *endpoint_results_, *fixed_up_dns_alias_results_);
  }

  void OnAsyncCompleted(size_t id, int error) {
    DCHECK_EQ(id_, id);
    id_ = 0;

    // Check that error information has been set and that the top-level error
    // code is valid.
    DCHECK(resolve_error_info_.error != ERR_IO_PENDING);
    DCHECK(error == OK || error == ERR_NAME_NOT_RESOLVED ||
           error == ERR_DNS_NAME_HTTPS_ONLY);

    DCHECK(!complete_);
    complete_ = true;

    DCHECK(callback_);
    std::move(callback_).Run(error);
  }

  const absl::variant<url::SchemeHostPort, HostPortPair>& request_endpoint()
      const {
    return request_endpoint_;
  }

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  const ResolveHostParameters& parameters() const { return parameters_; }

  int host_resolver_flags() const { return host_resolver_flags_; }

  size_t id() { return id_; }

  RequestPriority priority() const { return priority_; }

  void set_id(size_t id) {
    DCHECK_GT(id, 0u);
    DCHECK_EQ(0u, id_);

    id_ = id;
  }

  bool complete() { return complete_; }

  // Similar get GetAddressResults() and GetResolveErrorInfo(), but only exposed
  // through the HostResolver::ResolveHostRequest interface, and don't have the
  // DCHECKs that `complete_` is true.
  const absl::optional<AddressList>& address_results() const {
    return address_results_;
  }
  ResolveErrorInfo resolve_error_info() const { return resolve_error_info_; }

 private:
  AddressList FixupAddressList(const AddressList& list) {
    // Filter address family by query type and set request port if response port
    // is default.
    AddressList corrected;
    for (const IPEndPoint& endpoint : list.endpoints()) {
      DCHECK_NE(endpoint.GetFamily(), ADDRESS_FAMILY_UNSPECIFIED);
      if (parameters_.dns_query_type == DnsQueryType::UNSPECIFIED ||
          parameters_.dns_query_type ==
              AddressFamilyToDnsQueryType(endpoint.GetFamily())) {
        if (endpoint.port() == 0) {
          corrected.push_back(
              IPEndPoint(endpoint.address(), GetPort(request_endpoint_)));
        } else {
          corrected.push_back(endpoint);
        }
      }
    }

    // Copy over aliases and set to request hostname if empty.
    corrected.SetDnsAliases(list.dns_aliases());
    if (corrected.dns_aliases().empty())
      corrected.SetDnsAliases({std::string(GetHostname(request_endpoint_))});

    return corrected;
  }

  const absl::variant<url::SchemeHostPort, HostPortPair> request_endpoint_;
  const NetworkIsolationKey network_isolation_key_;
  const ResolveHostParameters parameters_;
  RequestPriority priority_;
  int host_resolver_flags_;

  absl::optional<AddressList> address_results_;
  absl::optional<std::vector<HostResolverEndpointResult>> endpoint_results_;
  absl::optional<std::set<std::string>> fixed_up_dns_alias_results_;
  absl::optional<HostCache::EntryStaleness> staleness_;
  ResolveErrorInfo resolve_error_info_;

  // Used while stored with the resolver for async resolution.  Otherwise 0.
  size_t id_;

  CompletionOnceCallback callback_;
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<MockHostResolverBase> resolver_;
  bool complete_;
};

class MockHostResolverBase::ProbeRequestImpl
    : public HostResolver::ProbeRequest {
 public:
  explicit ProbeRequestImpl(base::WeakPtr<MockHostResolverBase> resolver)
      : resolver_(std::move(resolver)) {}

  ProbeRequestImpl(const ProbeRequestImpl&) = delete;
  ProbeRequestImpl& operator=(const ProbeRequestImpl&) = delete;

  ~ProbeRequestImpl() override {
    if (resolver_ && resolver_->doh_probe_request_ == this)
      resolver_->doh_probe_request_ = nullptr;
  }

  int Start() override {
    DCHECK(resolver_);
    DCHECK(!resolver_->doh_probe_request_);

    resolver_->doh_probe_request_ = this;

    return ERR_IO_PENDING;
  }

 private:
  base::WeakPtr<MockHostResolverBase> resolver_;
};

class MockHostResolverBase::MdnsListenerImpl
    : public HostResolver::MdnsListener {
 public:
  MdnsListenerImpl(const HostPortPair& host,
                   DnsQueryType query_type,
                   base::WeakPtr<MockHostResolverBase> resolver)
      : host_(host),
        query_type_(query_type),
        delegate_(nullptr),
        resolver_(resolver) {
    DCHECK_NE(DnsQueryType::UNSPECIFIED, query_type_);
    DCHECK(resolver_);
  }

  ~MdnsListenerImpl() override {
    if (resolver_)
      resolver_->RemoveCancelledListener(this);
  }

  int Start(Delegate* delegate) override {
    DCHECK(delegate);
    DCHECK(!delegate_);
    DCHECK(resolver_);

    delegate_ = delegate;
    resolver_->AddListener(this);

    return OK;
  }

  void TriggerAddressResult(MdnsListenerUpdateType update_type,
                            IPEndPoint address) {
    delegate_->OnAddressResult(update_type, query_type_, std::move(address));
  }

  void TriggerTextResult(MdnsListenerUpdateType update_type,
                         std::vector<std::string> text_records) {
    delegate_->OnTextResult(update_type, query_type_, std::move(text_records));
  }

  void TriggerHostnameResult(MdnsListenerUpdateType update_type,
                             HostPortPair host) {
    delegate_->OnHostnameResult(update_type, query_type_, std::move(host));
  }

  void TriggerUnhandledResult(MdnsListenerUpdateType update_type) {
    delegate_->OnUnhandledResult(update_type, query_type_);
  }

  const HostPortPair& host() const { return host_; }
  DnsQueryType query_type() const { return query_type_; }

 private:
  const HostPortPair host_;
  const DnsQueryType query_type_;

  raw_ptr<Delegate> delegate_;

  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding listener objects.
  base::WeakPtr<MockHostResolverBase> resolver_;
};

MockHostResolverBase::RuleResolver::RuleKey::RuleKey() = default;

MockHostResolverBase::RuleResolver::RuleKey::~RuleKey() = default;

MockHostResolverBase::RuleResolver::RuleKey::RuleKey(const RuleKey&) = default;

MockHostResolverBase::RuleResolver::RuleKey&
MockHostResolverBase::RuleResolver::RuleKey::operator=(const RuleKey&) =
    default;

MockHostResolverBase::RuleResolver::RuleKey::RuleKey(RuleKey&&) = default;

MockHostResolverBase::RuleResolver::RuleKey&
MockHostResolverBase::RuleResolver::RuleKey::operator=(RuleKey&&) = default;

MockHostResolverBase::RuleResolver::RuleResolver(
    absl::optional<RuleResult> default_result)
    : default_result_(std::move(default_result)) {}

MockHostResolverBase::RuleResolver::~RuleResolver() = default;

MockHostResolverBase::RuleResolver::RuleResolver(const RuleResolver&) = default;

MockHostResolverBase::RuleResolver&
MockHostResolverBase::RuleResolver::operator=(const RuleResolver&) = default;

MockHostResolverBase::RuleResolver::RuleResolver(RuleResolver&&) = default;

MockHostResolverBase::RuleResolver&
MockHostResolverBase::RuleResolver::operator=(RuleResolver&&) = default;

const MockHostResolverBase::RuleResolver::RuleResult&
MockHostResolverBase::RuleResolver::Resolve(
    const absl::variant<url::SchemeHostPort, HostPortPair>& request_endpoint,
    DnsQueryTypeSet request_types,
    HostResolverSource request_source) const {
  for (const auto& rule : rules_) {
    const RuleKey& key = rule.first;
    const RuleResult& result = rule.second;

    if (absl::holds_alternative<RuleKey::NoScheme>(key.scheme) &&
        absl::holds_alternative<url::SchemeHostPort>(request_endpoint)) {
      continue;
    }

    if (key.port.has_value() && key.port.value() != GetPort(request_endpoint)) {
      continue;
    }

    DCHECK(!key.query_type.has_value() ||
           key.query_type.value() != DnsQueryType::UNSPECIFIED);
    if (key.query_type.has_value() &&
        !request_types.Has(key.query_type.value())) {
      continue;
    }

    if (key.query_source.has_value() &&
        request_source != key.query_source.value()) {
      continue;
    }

    if (absl::holds_alternative<RuleKey::Scheme>(key.scheme) &&
        (!absl::holds_alternative<url::SchemeHostPort>(request_endpoint) ||
         GetScheme(request_endpoint) !=
             absl::get<RuleKey::Scheme>(key.scheme))) {
      continue;
    }

    if (!base::MatchPattern(GetHostname(request_endpoint),
                            key.hostname_pattern)) {
      continue;
    }

    return result;
  }

  if (default_result_)
    return default_result_.value();

  NOTREACHED() << "Request " << GetHostname(request_endpoint)
               << " did not match any MockHostResolver rules.";
  static const RuleResult kUnexpected = ERR_UNEXPECTED;
  return kUnexpected;
}

void MockHostResolverBase::RuleResolver::ClearRules() {
  rules_.clear();
}

// static
MockHostResolverBase::RuleResolver::RuleResult
MockHostResolverBase::RuleResolver::GetLocalhostResult() {
  return AddressList::CreateFromIPAddress(IPAddress::IPv4Localhost(),
                                          /*port=*/0);
}

void MockHostResolverBase::RuleResolver::AddRule(RuleKey key,
                                                 RuleResult result) {
  // Literals are always resolved to themselves by MockHostResolverBase,
  // consequently we do not support remapping them.
  IPAddress ip_address;
  DCHECK(!ip_address.AssignFromIPLiteral(key.hostname_pattern));

  CHECK(rules_.emplace(std::move(key), std::move(result)).second)
      << "Duplicate rule key";
}

void MockHostResolverBase::RuleResolver::AddRule(RuleKey key,
                                                 base::StringPiece ip_literal) {
  AddressList results;
  CHECK_EQ(ParseAddressList(ip_literal, /*dns_aliases=*/{}, &results), OK);

  AddRule(std::move(key), std::move(results));
}

void MockHostResolverBase::RuleResolver::AddRule(
    base::StringPiece hostname_pattern,
    RuleResult result) {
  RuleKey key;
  key.hostname_pattern = std::string(hostname_pattern);
  AddRule(std::move(key), std::move(result));
}

void MockHostResolverBase::RuleResolver::AddRule(
    base::StringPiece hostname_pattern,
    base::StringPiece ip_literal) {
  AddressList results;
  CHECK_EQ(ParseAddressList(ip_literal, /*dns_aliases=*/{}, &results), OK);

  AddRule(hostname_pattern, std::move(results));
}

void MockHostResolverBase::RuleResolver::AddRule(
    base::StringPiece hostname_pattern,
    Error error) {
  RuleKey key;
  key.hostname_pattern = std::string(hostname_pattern);

  AddRule(std::move(key), error);
}

void MockHostResolverBase::RuleResolver::AddIPLiteralRule(
    base::StringPiece hostname_pattern,
    base::StringPiece ip_literal,
    base::StringPiece canonical_name) {
  RuleKey key;
  key.hostname_pattern = std::string(hostname_pattern);

  std::vector<std::string> aliases;
  if (!canonical_name.empty())
    aliases.emplace_back(canonical_name);

  AddressList results;
  CHECK_EQ(ParseAddressList(ip_literal, aliases, &results), OK);

  AddRule(std::move(key), std::move(results));
}

void MockHostResolverBase::RuleResolver::AddIPLiteralRuleWithDnsAliases(
    base::StringPiece hostname_pattern,
    base::StringPiece ip_literal,
    std::vector<std::string> dns_aliases) {
  AddressList results;
  CHECK_EQ(ParseAddressList(ip_literal, dns_aliases, &results), OK);

  AddRule(hostname_pattern, std::move(results));
}

void MockHostResolverBase::RuleResolver::AddIPLiteralRuleWithDnsAliases(
    base::StringPiece hostname_pattern,
    base::StringPiece ip_literal,
    std::set<std::string> dns_aliases) {
  std::vector<std::string> aliases_vector;
  base::ranges::move(dns_aliases, std::back_inserter(aliases_vector));

  AddIPLiteralRuleWithDnsAliases(hostname_pattern, ip_literal,
                                 std::move(aliases_vector));
}

void MockHostResolverBase::RuleResolver::AddSimulatedFailure(
    base::StringPiece hostname_pattern) {
  AddRule(hostname_pattern, ERR_NAME_NOT_RESOLVED);
}

void MockHostResolverBase::RuleResolver::AddSimulatedTimeoutFailure(
    base::StringPiece hostname_pattern) {
  AddRule(hostname_pattern, ERR_DNS_TIMED_OUT);
}

void MockHostResolverBase::RuleResolver::AddRuleWithFlags(
    base::StringPiece host_pattern,
    base::StringPiece ip_literal,
    HostResolverFlags /*flags*/,
    std::vector<std::string> dns_aliases) {
  AddressList results;
  CHECK_EQ(ParseAddressList(ip_literal, dns_aliases, &results), OK);

  AddRule(host_pattern, std::move(results));
}

MockHostResolverBase::~MockHostResolverBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Sanity check that pending requests are always cleaned up, by waiting for
  // completion, manually cancelling, or calling OnShutdown().
  DCHECK(requests_.empty());
}

void MockHostResolverBase::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Cancel all pending requests.
  for (auto& request : requests_) {
    request.second->DetachFromResolver();
  }
  requests_.clear();

  // Prevent future requests by clearing resolution rules and the cache.
  rule_resolver_.ClearRules();
  cache_ = nullptr;

  doh_probe_request_ = nullptr;
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MockHostResolverBase::CreateRequest(
    url::SchemeHostPort host,
    NetworkIsolationKey network_isolation_key,
    NetLogWithSource net_log,
    absl::optional<ResolveHostParameters> optional_parameters) {
  return std::make_unique<RequestImpl>(std::move(host), network_isolation_key,
                                       optional_parameters, AsWeakPtr());
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MockHostResolverBase::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& source_net_log,
    const absl::optional<ResolveHostParameters>& optional_parameters) {
  return std::make_unique<RequestImpl>(host, network_isolation_key,
                                       optional_parameters, AsWeakPtr());
}

std::unique_ptr<HostResolver::ProbeRequest>
MockHostResolverBase::CreateDohProbeRequest() {
  return std::make_unique<ProbeRequestImpl>(AsWeakPtr());
}

std::unique_ptr<HostResolver::MdnsListener>
MockHostResolverBase::CreateMdnsListener(const HostPortPair& host,
                                         DnsQueryType query_type) {
  return std::make_unique<MdnsListenerImpl>(host, query_type, AsWeakPtr());
}

HostCache* MockHostResolverBase::GetHostCache() {
  return cache_.get();
}

int MockHostResolverBase::LoadIntoCache(
    const absl::variant<url::SchemeHostPort, HostPortPair>& endpoint,
    const NetworkIsolationKey& network_isolation_key,
    const absl::optional<ResolveHostParameters>& optional_parameters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(cache_);

  ResolveHostParameters parameters =
      optional_parameters.value_or(ResolveHostParameters());

  AddressList addresses;
  absl::optional<HostCache::EntryStaleness> stale_info;
  int rv = ResolveFromIPLiteralOrCache(
      endpoint, network_isolation_key, parameters.dns_query_type,
      ParametersToHostResolverFlags(parameters), parameters.source,
      parameters.cache_usage, &addresses, &stale_info);
  if (rv != ERR_DNS_CACHE_MISS) {
    // Request already in cache (or IP literal). No need to load it.
    return rv;
  }

  // Just like the real resolver, refuse to do anything with invalid
  // hostnames.
  if (!IsValidDNSDomain(GetHostname(endpoint)))
    return ERR_NAME_NOT_RESOLVED;

  RequestImpl request(endpoint, network_isolation_key, optional_parameters,
                      AsWeakPtr());
  return DoSynchronousResolution(request);
}

void MockHostResolverBase::ResolveAllPending() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ondemand_mode_);
  for (auto i = requests_.begin(); i != requests_.end(); ++i) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MockHostResolverBase::ResolveNow,
                                  AsWeakPtr(), i->first));
  }
}

size_t MockHostResolverBase::last_id() {
  if (requests_.empty())
    return 0;
  return requests_.rbegin()->first;
}

void MockHostResolverBase::ResolveNow(size_t id) {
  auto it = requests_.find(id);
  if (it == requests_.end())
    return;  // was canceled

  RequestImpl* req = it->second;
  requests_.erase(it);

  int error = DoSynchronousResolution(*req);
  req->OnAsyncCompleted(id, error);
}

void MockHostResolverBase::DetachRequest(size_t id) {
  auto it = requests_.find(id);
  CHECK(it != requests_.end());
  requests_.erase(it);
}

base::StringPiece MockHostResolverBase::request_host(size_t id) {
  DCHECK(request(id));
  return GetHostname(request(id)->request_endpoint());
}

RequestPriority MockHostResolverBase::request_priority(size_t id) {
  DCHECK(request(id));
  return request(id)->priority();
}

const NetworkIsolationKey& MockHostResolverBase::request_network_isolation_key(
    size_t id) {
  DCHECK(request(id));
  return request(id)->network_isolation_key();
}

void MockHostResolverBase::ResolveOnlyRequestNow() {
  DCHECK_EQ(1u, requests_.size());
  ResolveNow(requests_.begin()->first);
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type,
    const IPEndPoint& address_result) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerAddressResult(update_type, address_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type,
    const std::vector<std::string>& text_result) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerTextResult(update_type, text_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type,
    const HostPortPair& host_result) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerHostnameResult(update_type, host_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerUnhandledResult(update_type);
  }
}

MockHostResolverBase::RequestImpl* MockHostResolverBase::request(size_t id) {
  RequestMap::iterator request = requests_.find(id);
  CHECK(request != requests_.end());
  CHECK_EQ(request->second->id(), id);
  return (*request).second;
}

// start id from 1 to distinguish from NULL RequestHandle
MockHostResolverBase::MockHostResolverBase(bool use_caching,
                                           int cache_invalidation_num,
                                           RuleResolver rule_resolver)
    : last_request_priority_(DEFAULT_PRIORITY),
      last_secure_dns_policy_(SecureDnsPolicy::kAllow),
      synchronous_mode_(false),
      ondemand_mode_(false),
      rule_resolver_(std::move(rule_resolver)),
      initial_cache_invalidation_num_(cache_invalidation_num),
      next_request_id_(1),
      num_resolve_(0),
      num_resolve_from_cache_(0),
      num_non_local_resolves_(0),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  if (use_caching)
    cache_ = std::make_unique<HostCache>(kMaxCacheEntries);
  else
    DCHECK_GE(0, cache_invalidation_num);
}

int MockHostResolverBase::Resolve(RequestImpl* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  last_request_priority_ = request->parameters().initial_priority;
  last_request_network_isolation_key_ = request->network_isolation_key();
  last_secure_dns_policy_ = request->parameters().secure_dns_policy;
  num_resolve_++;
  AddressList addresses;
  absl::optional<HostCache::EntryStaleness> stale_info;
  // TODO(crbug.com/1264933): Allow caching `ConnectionEndpoint` results.
  int rv = ResolveFromIPLiteralOrCache(
      request->request_endpoint(), request->network_isolation_key(),
      request->parameters().dns_query_type, request->host_resolver_flags(),
      request->parameters().source, request->parameters().cache_usage,
      &addresses, &stale_info);

  if (rv == OK && !request->parameters().is_speculative) {
    request->SetAddressResults(addresses, std::move(stale_info));
  } else {
    request->SetError(rv);
  }

  if (rv != ERR_DNS_CACHE_MISS ||
      request->parameters().source == HostResolverSource::LOCAL_ONLY) {
    return SquashErrorCode(rv);
  }

  // Just like the real resolver, refuse to do anything with invalid
  // hostnames.
  if (!IsValidDNSDomain(GetHostname(request->request_endpoint()))) {
    request->SetError(ERR_NAME_NOT_RESOLVED);
    return ERR_NAME_NOT_RESOLVED;
  }

  if (synchronous_mode_)
    return DoSynchronousResolution(*request);

  // Store the request for asynchronous resolution
  size_t id = next_request_id_++;
  request->set_id(id);
  requests_[id] = request;

  if (!ondemand_mode_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockHostResolverBase::ResolveNow, AsWeakPtr(), id));
  }

  return ERR_IO_PENDING;
}

int MockHostResolverBase::ResolveFromIPLiteralOrCache(
    const absl::variant<url::SchemeHostPort, HostPortPair>& endpoint,
    const NetworkIsolationKey& network_isolation_key,
    DnsQueryType dns_query_type,
    HostResolverFlags flags,
    HostResolverSource source,
    HostResolver::ResolveHostParameters::CacheUsage cache_usage,
    AddressList* addresses,
    absl::optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(addresses);
  DCHECK(out_stale_info);
  *out_stale_info = absl::nullopt;

  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(GetHostname(endpoint))) {
    const DnsQueryType desired_address_query =
        AddressFamilyToDnsQueryType(GetAddressFamily(ip_address));
    DCHECK_NE(desired_address_query, DnsQueryType::UNSPECIFIED);

    // This matches the behavior HostResolverImpl.
    if (dns_query_type != DnsQueryType::UNSPECIFIED &&
        dns_query_type != desired_address_query) {
      return ERR_NAME_NOT_RESOLVED;
    }

    *addresses =
        AddressList::CreateFromIPAddress(ip_address, GetPort(endpoint));
    if (flags & HOST_RESOLVER_CANONNAME)
      addresses->SetDefaultCanonicalName();
    return OK;
  }

  // Immediately resolve any "localhost" or recognized similar names.
  if (IsAddressType(dns_query_type) &&
      ResolveLocalHostname(GetHostname(endpoint), addresses)) {
    return OK;
  }
  int rv = ERR_DNS_CACHE_MISS;
  bool cache_allowed =
      cache_usage == HostResolver::ResolveHostParameters::CacheUsage::ALLOWED ||
      cache_usage ==
          HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  if (cache_.get() && cache_allowed) {
    // Local-only requests search the cache for non-local-only results.
    HostResolverSource effective_source =
        source == HostResolverSource::LOCAL_ONLY ? HostResolverSource::ANY
                                                 : source;
    HostCache::Key key(GetCacheHost(endpoint), dns_query_type, flags,
                       effective_source, network_isolation_key);
    const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;
    HostCache::EntryStaleness stale_info = HostCache::kNotStale;
    if (cache_usage ==
        HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED) {
      cache_result = cache_->LookupStale(key, tick_clock_->NowTicks(),
                                         &stale_info, true /* ignore_secure */);
    } else {
      cache_result = cache_->Lookup(key, tick_clock_->NowTicks(),
                                    true /* ignore_secure */);
    }
    if (cache_result) {
      rv = cache_result->second.error();
      if (rv == OK) {
        *addresses = AddressList::CopyWithPort(
            cache_result->second.legacy_addresses().value(), GetPort(endpoint));
        *out_stale_info = std::move(stale_info);
      }

      auto cache_invalidation_iterator = cache_invalidation_nums_.find(key);
      if (cache_invalidation_iterator != cache_invalidation_nums_.end()) {
        DCHECK_LE(1, cache_invalidation_iterator->second);
        cache_invalidation_iterator->second--;
        if (cache_invalidation_iterator->second == 0) {
          HostCache::Entry new_entry(cache_result->second);
          cache_->Set(key, new_entry, tick_clock_->NowTicks(),
                      base::TimeDelta());
          cache_invalidation_nums_.erase(cache_invalidation_iterator);
        }
      }
    }
  }
  return rv;
}

int MockHostResolverBase::DoSynchronousResolution(RequestImpl& request) {
  ++num_non_local_resolves_;

  const RuleResolver::RuleResult& result = rule_resolver_.Resolve(
      request.request_endpoint(), request.parameters().dns_query_type,
      request.parameters().source);

  int error = ERR_UNEXPECTED;
  absl::optional<HostCache::Entry> cache_entry;

  if (absl::holds_alternative<AddressList>(result)) {
    const AddressList& address_results = absl::get<AddressList>(result);
    request.SetAddressResults(address_results,
                              /*staleness=*/absl::nullopt);
    error = request.resolve_error_info().error;
    cache_entry =
        request.address_results()
            ? HostCache::Entry(error, *request.address_results(),
                               HostCache::Entry::SOURCE_UNKNOWN)
            : HostCache::Entry(error, HostCache::Entry::SOURCE_UNKNOWN);
  } else if (absl::holds_alternative<std::vector<HostResolverEndpointResult>>(
                 result)) {
    const auto& endpoint_results =
        absl::get<std::vector<HostResolverEndpointResult>>(result);
    request.SetEndpointResults(endpoint_results);
    // TODO(crbug.com/1264933): Change `error` on empty results?
    error = OK;
    // TODO(crbug.com/1264933): Save result to cache.
  } else {
    DCHECK(absl::holds_alternative<RuleResolver::ErrorResult>(result));
    error = absl::get<RuleResolver::ErrorResult>(result);
    request.SetError(error);
    cache_entry.emplace(error, HostCache::Entry::SOURCE_UNKNOWN);
  }

  if (cache_.get() && cache_entry.has_value()) {
    DCHECK(cache_entry.has_value());
    HostCache::Key key(
        GetCacheHost(request.request_endpoint()),
        request.parameters().dns_query_type, request.host_resolver_flags(),
        request.parameters().source, request.network_isolation_key());
    // Storing a failure with TTL 0 so that it overwrites previous value.
    base::TimeDelta ttl;
    if (error == OK) {
      ttl = base::Seconds(kCacheEntryTTLSeconds);
      if (initial_cache_invalidation_num_ > 0)
        cache_invalidation_nums_[key] = initial_cache_invalidation_num_;
    }
    cache_->Set(key, cache_entry.value(), tick_clock_->NowTicks(), ttl);
  }

  return SquashErrorCode(error);
}

void MockHostResolverBase::AddListener(MdnsListenerImpl* listener) {
  listeners_.insert(listener);
}

void MockHostResolverBase::RemoveCancelledListener(MdnsListenerImpl* listener) {
  listeners_.erase(listener);
}

MockHostResolverFactory::MockHostResolverFactory(
    MockHostResolverBase::RuleResolver rules,
    bool use_caching,
    int cache_invalidation_num)
    : rules_(std::move(rules)),
      use_caching_(use_caching),
      cache_invalidation_num_(cache_invalidation_num) {}

MockHostResolverFactory::~MockHostResolverFactory() = default;

std::unique_ptr<HostResolver> MockHostResolverFactory::CreateResolver(
    HostResolverManager* manager,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  DCHECK(host_mapping_rules.empty());

  // Explicit new to access private constructor.
  auto resolver = base::WrapUnique(new MockHostResolverBase(
      enable_caching && use_caching_, cache_invalidation_num_, rules_));
  return resolver;
}

std::unique_ptr<HostResolver> MockHostResolverFactory::CreateStandaloneResolver(
    NetLog* net_log,
    const HostResolver::ManagerOptions& options,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  return CreateResolver(nullptr, host_mapping_rules, enable_caching);
}

//-----------------------------------------------------------------------------

RuleBasedHostResolverProc::Rule::Rule(ResolverType resolver_type,
                                      const std::string& host_pattern,
                                      AddressFamily address_family,
                                      HostResolverFlags host_resolver_flags,
                                      const std::string& replacement,
                                      std::vector<std::string> dns_aliases,
                                      int latency_ms)
    : resolver_type(resolver_type),
      host_pattern(host_pattern),
      address_family(address_family),
      host_resolver_flags(host_resolver_flags),
      replacement(replacement),
      dns_aliases(std::move(dns_aliases)),
      latency_ms(latency_ms) {
  DCHECK(this->dns_aliases != std::vector<std::string>({""}));
}

RuleBasedHostResolverProc::Rule::Rule(const Rule& other) = default;

RuleBasedHostResolverProc::Rule::~Rule() = default;

RuleBasedHostResolverProc::RuleBasedHostResolverProc(HostResolverProc* previous,
                                                     bool allow_fallback)
    : HostResolverProc(previous, allow_fallback),
      modifications_allowed_(true) {}

void RuleBasedHostResolverProc::AddRule(const std::string& host_pattern,
                                        const std::string& replacement) {
  AddRuleForAddressFamily(host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
                          replacement);
}

void RuleBasedHostResolverProc::AddRuleForAddressFamily(
    const std::string& host_pattern,
    AddressFamily address_family,
    const std::string& replacement) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, address_family, flags,
            replacement, {} /* dns_aliases */, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddRuleWithFlags(
    const std::string& host_pattern,
    const std::string& replacement,
    HostResolverFlags flags,
    std::vector<std::string> dns_aliases) {
  DCHECK(!replacement.empty());
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, std::move(dns_aliases), 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddIPLiteralRule(
    const std::string& host_pattern,
    const std::string& ip_literal,
    const std::string& canonical_name) {
  // Literals are always resolved to themselves by HostResolverImpl,
  // consequently we do not support remapping them.
  IPAddress ip_address;
  DCHECK(!ip_address.AssignFromIPLiteral(host_pattern));
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  std::vector<std::string> aliases;
  if (!canonical_name.empty()) {
    flags |= HOST_RESOLVER_CANONNAME;
    aliases.emplace_back(canonical_name);
  }

  Rule rule(Rule::kResolverTypeIPLiteral, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, ip_literal, std::move(aliases),
            0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddIPLiteralRuleWithDnsAliases(
    const std::string& host_pattern,
    const std::string& ip_literal,
    std::vector<std::string> dns_aliases) {
  // Literals are always resolved to themselves by HostResolverImpl,
  // consequently we do not support remapping them.
  IPAddress ip_address;
  DCHECK(!ip_address.AssignFromIPLiteral(host_pattern));
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  if (!dns_aliases.empty())
    flags |= HOST_RESOLVER_CANONNAME;

  Rule rule(Rule::kResolverTypeIPLiteral, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, ip_literal,
            std::move(dns_aliases), 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddRuleWithLatency(
    const std::string& host_pattern,
    const std::string& replacement,
    int latency_ms) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, /*dns_aliases=*/{}, latency_ms);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AllowDirectLookup(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), /*dns_aliases=*/{}, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddSimulatedFailure(
    const std::string& host_pattern,
    HostResolverFlags flags) {
  Rule rule(Rule::kResolverTypeFail, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), /*dns_aliases=*/{}, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddSimulatedTimeoutFailure(
    const std::string& host_pattern,
    HostResolverFlags flags) {
  Rule rule(Rule::kResolverTypeFailTimeout, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, std::string(),
            /*dns_aliases=*/{}, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::ClearRules() {
  CHECK(modifications_allowed_);
  base::AutoLock lock(rule_lock_);
  rules_.clear();
}

void RuleBasedHostResolverProc::DisableModifications() {
  modifications_allowed_ = false;
}

RuleBasedHostResolverProc::RuleList RuleBasedHostResolverProc::GetRules() {
  RuleList rv;
  {
    base::AutoLock lock(rule_lock_);
    rv = rules_;
  }
  return rv;
}

int RuleBasedHostResolverProc::Resolve(const std::string& host,
                                       AddressFamily address_family,
                                       HostResolverFlags host_resolver_flags,
                                       AddressList* addrlist,
                                       int* os_error) {
  base::AutoLock lock(rule_lock_);
  RuleList::iterator r;
  for (r = rules_.begin(); r != rules_.end(); ++r) {
    bool matches_address_family =
        r->address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        r->address_family == address_family;
    // Ignore HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6, since it should
    // have no impact on whether a rule matches.
    HostResolverFlags flags =
        host_resolver_flags & ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
    // Flags match if all of the bitflags in host_resolver_flags are enabled
    // in the rule's host_resolver_flags. However, the rule may have additional
    // flags specified, in which case the flags should still be considered a
    // match.
    bool matches_flags = (r->host_resolver_flags & flags) == flags;
    if (matches_flags && matches_address_family &&
        base::MatchPattern(host, r->host_pattern)) {
      if (r->latency_ms != 0) {
        base::PlatformThread::Sleep(base::Milliseconds(r->latency_ms));
      }

      // Remap to a new host.
      const std::string& effective_host =
          r->replacement.empty() ? host : r->replacement;

      // Apply the resolving function to the remapped hostname.
      switch (r->resolver_type) {
        case Rule::kResolverTypeFail:
          return ERR_NAME_NOT_RESOLVED;
        case Rule::kResolverTypeFailTimeout:
          return ERR_DNS_TIMED_OUT;
        case Rule::kResolverTypeSystem:
#if BUILDFLAG(IS_WIN)
          EnsureWinsockInit();
#endif
          return SystemHostResolverCall(effective_host, address_family,
                                        host_resolver_flags, addrlist,
                                        os_error);
        case Rule::kResolverTypeIPLiteral: {
          AddressList raw_addr_list;
          std::vector<std::string> aliases;
          aliases = (!r->dns_aliases.empty())
                        ? r->dns_aliases
                        : std::vector<std::string>({host});
          int result =
              ParseAddressList(effective_host, aliases, &raw_addr_list);
          // Filter out addresses with the wrong family.
          *addrlist = AddressList();
          for (const auto& address : raw_addr_list) {
            if (address_family == ADDRESS_FAMILY_UNSPECIFIED ||
                address_family == address.GetFamily()) {
              addrlist->push_back(address);
            }
          }
          addrlist->SetDnsAliases(raw_addr_list.dns_aliases());

          if (result == OK && addrlist->empty())
            return ERR_NAME_NOT_RESOLVED;
          return result;
        }
        default:
          NOTREACHED();
          return ERR_UNEXPECTED;
      }
    }
  }

  return ResolveUsingPrevious(host, address_family, host_resolver_flags,
                              addrlist, os_error);
}

RuleBasedHostResolverProc::~RuleBasedHostResolverProc() = default;

void RuleBasedHostResolverProc::AddRuleInternal(const Rule& rule) {
  Rule fixed_rule = rule;
  // SystemResolverProc expects valid DNS addresses.
  // So for kResolverTypeSystem rules:
  // * If the replacement is an IP address, switch to an IP literal rule.
  // * If it's a non-empty invalid domain name, switch to a fail rule (Empty
  // domain names mean use a direct lookup).
  if (fixed_rule.resolver_type == Rule::kResolverTypeSystem) {
    IPAddress ip_address;
    bool valid_address = ip_address.AssignFromIPLiteral(fixed_rule.replacement);
    if (valid_address) {
      fixed_rule.resolver_type = Rule::kResolverTypeIPLiteral;
    } else if (!fixed_rule.replacement.empty() &&
               !IsValidDNSDomain(fixed_rule.replacement)) {
      // TODO(mmenke): Can this be replaced with a DCHECK instead?
      fixed_rule.resolver_type = Rule::kResolverTypeFail;
    }
  }

  CHECK(modifications_allowed_);
  base::AutoLock lock(rule_lock_);
  rules_.push_back(fixed_rule);
}

RuleBasedHostResolverProc* CreateCatchAllHostResolverProc() {
  RuleBasedHostResolverProc* catchall =
      new RuleBasedHostResolverProc(/*previous=*/nullptr,
                                    /*allow_fallback=*/false);
  // Note that IPv6 lookups fail.
  catchall->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  // Next add a rules-based layer that the test controls.
  return new RuleBasedHostResolverProc(catchall, /*allow_fallback=*/false);
}

//-----------------------------------------------------------------------------

// Implementation of ResolveHostRequest that tracks cancellations when the
// request is destroyed after being started.
class HangingHostResolver::RequestImpl
    : public HostResolver::ResolveHostRequest,
      public HostResolver::ProbeRequest {
 public:
  explicit RequestImpl(base::WeakPtr<HangingHostResolver> resolver)
      : resolver_(resolver) {}

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override {
    if (is_running_ && resolver_)
      resolver_->num_cancellations_++;
  }

  int Start(CompletionOnceCallback callback) override { return Start(); }

  int Start() override {
    DCHECK(resolver_);
    is_running_ = true;
    return ERR_IO_PENDING;
  }

  const AddressList* GetAddressResults() const override { IMMEDIATE_CRASH(); }

  const std::vector<HostResolverEndpointResult>* GetEndpointResults()
      const override {
    IMMEDIATE_CRASH();
  }

  const absl::optional<std::vector<std::string>>& GetTextResults()
      const override {
    IMMEDIATE_CRASH();
  }

  const absl::optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    IMMEDIATE_CRASH();
  }

  const std::set<std::string>* GetDnsAliasResults() const override {
    IMMEDIATE_CRASH();
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    IMMEDIATE_CRASH();
  }

  const absl::optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    IMMEDIATE_CRASH();
  }

  void ChangeRequestPriority(RequestPriority priority) override {}

 private:
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<HangingHostResolver> resolver_;
  bool is_running_ = false;
};

HangingHostResolver::HangingHostResolver() = default;

HangingHostResolver::~HangingHostResolver() = default;

void HangingHostResolver::OnShutdown() {
  shutting_down_ = true;
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HangingHostResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkIsolationKey network_isolation_key,
    NetLogWithSource net_log,
    absl::optional<ResolveHostParameters> optional_parameters) {
  // TODO(crbug.com/1206799): Propagate scheme and make affect behavior.
  return CreateRequest(HostPortPair::FromSchemeHostPort(host),
                       network_isolation_key, net_log, optional_parameters);
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HangingHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& source_net_log,
    const absl::optional<ResolveHostParameters>& optional_parameters) {
  last_host_ = host;
  last_network_isolation_key_ = network_isolation_key;

  if (shutting_down_)
    return CreateFailingRequest(ERR_CONTEXT_SHUT_DOWN);

  if (optional_parameters &&
      optional_parameters.value().source == HostResolverSource::LOCAL_ONLY) {
    return CreateFailingRequest(ERR_DNS_CACHE_MISS);
  }

  return std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::ProbeRequest>
HangingHostResolver::CreateDohProbeRequest() {
  if (shutting_down_)
    return CreateFailingProbeRequest(ERR_CONTEXT_SHUT_DOWN);

  return std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr());
}

//-----------------------------------------------------------------------------

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc() = default;

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc(
    HostResolverProc* proc) {
  Init(proc);
}

ScopedDefaultHostResolverProc::~ScopedDefaultHostResolverProc() {
  HostResolverProc* old_proc =
      HostResolverProc::SetDefault(previous_proc_.get());
  // The lifetimes of multiple instances must be nested.
  CHECK_EQ(old_proc, current_proc_.get());
}

void ScopedDefaultHostResolverProc::Init(HostResolverProc* proc) {
  current_proc_ = proc;
  previous_proc_ = HostResolverProc::SetDefault(current_proc_.get());
  current_proc_->SetLastProc(previous_proc_.get());
}

}  // namespace net
