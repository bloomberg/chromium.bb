// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_cache.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/value_iterators.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/trace_constants.h"
#include "net/dns/host_resolver.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/log/net_log.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

#define CACHE_HISTOGRAM_TIME(name, time) \
  UMA_HISTOGRAM_LONG_TIMES("DNS.HostCache." name, time)

#define CACHE_HISTOGRAM_COUNT(name, count) \
  UMA_HISTOGRAM_COUNTS_1000("DNS.HostCache." name, count)

#define CACHE_HISTOGRAM_ENUM(name, value, max) \
  UMA_HISTOGRAM_ENUMERATION("DNS.HostCache." name, value, max)

// String constants for dictionary keys.
const char kSchemeKey[] = "scheme";
const char kHostnameKey[] = "hostname";
const char kPortKey[] = "port";
const char kDnsQueryTypeKey[] = "dns_query_type";
const char kFlagsKey[] = "flags";
const char kHostResolverSourceKey[] = "host_resolver_source";
const char kSecureKey[] = "secure";
const char kNetworkIsolationKeyKey[] = "network_isolation_key";
const char kExpirationKey[] = "expiration";
const char kTtlKey[] = "ttl";
const char kPinnedKey[] = "pinned";
const char kNetworkChangesKey[] = "network_changes";
const char kNetErrorKey[] = "net_error";
const char kIpEndpointsKey[] = "ip_endpoints";
const char kEndpointAddressKey[] = "endpoint_address";
const char kEndpointPortKey[] = "endpoint_port";
const char kEndpointMetadatasKey[] = "endpoint_metadatas";
const char kEndpointMetadataWeightKey[] = "endpoint_metadata_weight";
const char kEndpointMetadataValueKey[] = "endpoint_metadata_value";
const char kAliasesKey[] = "aliases";
const char kAddressesKey[] = "addresses";
const char kTextRecordsKey[] = "text_records";
const char kHostnameResultsKey[] = "hostname_results";
const char kHostPortsKey[] = "host_ports";

base::Value IpEndpointToValue(const IPEndPoint& endpoint) {
  base::Value::DictStorage dictionary;
  dictionary.emplace(kEndpointAddressKey, endpoint.ToStringWithoutPort());
  dictionary.emplace(kEndpointPortKey, int{endpoint.port()});
  return base::Value(std::move(dictionary));
}

absl::optional<IPEndPoint> IpEndpointFromValue(const base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  const std::string* ip_str = value.FindStringKey(kEndpointAddressKey);
  absl::optional<int> port = value.FindIntKey(kEndpointPortKey);

  if (!ip_str || !port ||
      !base::IsValueInRangeForNumericType<uint16_t>(port.value())) {
    return absl::nullopt;
  }

  IPAddress ip;
  if (!ip.AssignFromIPLiteral(*ip_str))
    return absl::nullopt;

  return IPEndPoint(ip, base::checked_cast<uint16_t>(port.value()));
}

base::Value EndpointMetadataPairToValue(
    const std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>& pair) {
  base::Value::DictStorage dictionary;
  dictionary.emplace(kEndpointMetadataWeightKey, int{pair.first});
  dictionary.emplace(kEndpointMetadataValueKey, pair.second.ToValue());
  return base::Value(std::move(dictionary));
}

absl::optional<std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>>
EndpointMetadataPairFromValue(const base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  absl::optional<int> priority = value.FindIntKey(kEndpointMetadataWeightKey);
  const base::Value* metadata_value = value.FindKey(kEndpointMetadataValueKey);

  if (!priority || !base::IsValueInRangeForNumericType<HttpsRecordPriority>(
                       priority.value())) {
    return absl::nullopt;
  }

  if (!metadata_value)
    return absl::nullopt;
  absl::optional<ConnectionEndpointMetadata> metadata =
      ConnectionEndpointMetadata::FromValue(*metadata_value);
  if (!metadata)
    return absl::nullopt;

  return std::make_pair(
      base::checked_cast<HttpsRecordPriority>(priority.value()),
      std::move(metadata).value());
}

bool AddressListFromListValue(const base::Value* value,
                              absl::optional<AddressList>* out_list) {
  if (!value) {
    out_list->reset();
    return true;
  }

  out_list->emplace();
  for (const auto& it : value->GetListDeprecated()) {
    IPAddress address;
    const std::string* addr_string = it.GetIfString();
    if (!addr_string || !address.AssignFromIPLiteral(*addr_string)) {
      return false;
    }
    out_list->value().push_back(IPEndPoint(address, 0));
  }
  return true;
}

template <typename T>
void MergeLists(absl::optional<T>* target, const absl::optional<T>& source) {
  if (target->has_value() && source) {
    target->value().insert(target->value().end(), source.value().begin(),
                           source.value().end());
  } else if (source) {
    *target = source;
  }
}

template <typename T>
void MergeContainers(absl::optional<T>& target,
                     const absl::optional<T>& source) {
  if (target.has_value() && source.has_value()) {
    target->insert(source->begin(), source->end());
  } else if (source) {
    target = source;
  }
}

// Used to reject empty and IP literal (whether or not surrounded by brackets)
// hostnames.
bool IsValidHostname(base::StringPiece hostname) {
  if (hostname.empty())
    return false;

  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(hostname) ||
      ParseURLHostnameToAddress(hostname, &ip_address)) {
    return false;
  }

  return true;
}

const std::string& GetHostname(
    const absl::variant<url::SchemeHostPort, std::string>& host) {
  const std::string* hostname;
  if (absl::holds_alternative<url::SchemeHostPort>(host)) {
    hostname = &absl::get<url::SchemeHostPort>(host).host();
  } else {
    DCHECK(absl::holds_alternative<std::string>(host));
    hostname = &absl::get<std::string>(host);
  }

  DCHECK(IsValidHostname(*hostname));
  return *hostname;
}

}  // namespace

// Used in histograms; do not modify existing values.
enum HostCache::SetOutcome : int {
  SET_INSERT = 0,
  SET_UPDATE_VALID = 1,
  SET_UPDATE_STALE = 2,
  MAX_SET_OUTCOME
};

// Used in histograms; do not modify existing values.
enum HostCache::LookupOutcome : int {
  LOOKUP_MISS_ABSENT = 0,
  LOOKUP_MISS_STALE = 1,
  LOOKUP_HIT_VALID = 2,
  LOOKUP_HIT_STALE = 3,
  MAX_LOOKUP_OUTCOME
};

// Used in histograms; do not modify existing values.
enum HostCache::EraseReason : int {
  ERASE_EVICT = 0,
  ERASE_CLEAR = 1,
  ERASE_DESTRUCT = 2,
  MAX_ERASE_REASON
};

HostCache::Key::Key(absl::variant<url::SchemeHostPort, std::string> host,
                    DnsQueryType dns_query_type,
                    HostResolverFlags host_resolver_flags,
                    HostResolverSource host_resolver_source,
                    const NetworkIsolationKey& network_isolation_key)
    : host(std::move(host)),
      dns_query_type(dns_query_type),
      host_resolver_flags(host_resolver_flags),
      host_resolver_source(host_resolver_source),
      network_isolation_key(network_isolation_key) {
  DCHECK(IsValidHostname(GetHostname(this->host)));
  if (absl::holds_alternative<url::SchemeHostPort>(this->host))
    DCHECK(absl::get<url::SchemeHostPort>(this->host).IsValid());
}

HostCache::Key::Key() = default;
HostCache::Key::Key(const Key& key) = default;
HostCache::Key::Key(Key&& key) = default;

HostCache::Key::~Key() = default;

HostCache::Entry::Entry(int error,
                        Source source,
                        absl::optional<base::TimeDelta> ttl)
    : error_(error), source_(source), ttl_(ttl.value_or(base::Seconds(-1))) {
  // If |ttl| has a value, must not be negative.
  DCHECK_GE(ttl.value_or(base::TimeDelta()), base::TimeDelta());
  DCHECK_NE(OK, error_);

  // host_cache.h defines its own `HttpsRecordPriority` due to
  // https_record_rdata.h not being allowed in the same places, but the types
  // should still be the same thing.
  static_assert(std::is_same<net::HttpsRecordPriority,
                             HostCache::Entry::HttpsRecordPriority>::value,
                "`net::HttpsRecordPriority` and "
                "`HostCache::Entry::HttpsRecordPriority` must be same type");
}

HostCache::Entry::Entry(const Entry& entry) = default;

HostCache::Entry::Entry(Entry&& entry) = default;

HostCache::Entry::~Entry() = default;

absl::optional<std::vector<HostResolverEndpointResult>>
HostCache::Entry::GetEndpoints() const {
  if (!ip_endpoints_.has_value())
    return absl::nullopt;

  std::vector<HostResolverEndpointResult> endpoints;

  if (ip_endpoints_.value().empty())
    return endpoints;

  if (endpoint_metadatas_.has_value()) {
    HttpsRecordPriority last_priority = 0;
    for (const auto& metadata : endpoint_metadatas_.value()) {
      // Ensure metadatas are iterated in priority order.
      DCHECK_GE(metadata.first, last_priority);
      last_priority = metadata.first;

      endpoints.emplace_back();
      endpoints.back().ip_endpoints = ip_endpoints_.value();
      endpoints.back().metadata = metadata.second;
    }
  }

  // Add a final non-protocol endpoint at the end.
  endpoints.emplace_back();
  endpoints.back().ip_endpoints = ip_endpoints_.value();

  return endpoints;
}

absl::optional<base::TimeDelta> HostCache::Entry::GetOptionalTtl() const {
  if (has_ttl())
    return ttl();
  else
    return absl::nullopt;
}

// static
HostCache::Entry HostCache::Entry::MergeEntries(Entry front, Entry back) {
  // Only expected to merge OK or ERR_NAME_NOT_RESOLVED results.
  DCHECK(front.error() == OK || front.error() == ERR_NAME_NOT_RESOLVED);
  DCHECK(back.error() == OK || back.error() == ERR_NAME_NOT_RESOLVED);

  // Build results in |front| to preserve unmerged fields.

  front.error_ =
      front.error() == OK || back.error() == OK ? OK : ERR_NAME_NOT_RESOLVED;

  MergeLists(&front.ip_endpoints_, back.ip_endpoints_);
  MergeContainers(front.endpoint_metadatas_, back.endpoint_metadatas_);
  MergeContainers(front.aliases_, back.aliases_);
  front.MergeAddressesFrom(back);
  MergeLists(&front.text_records_, back.text_records());
  MergeLists(&front.hostnames_, back.hostnames());
  MergeLists(&front.experimental_results_, back.experimental_results());

  // The DNS aliases include the canonical name(s), if any, each as the
  // first entry in the field, which is an optional vector. If |front| has
  // a canonical name, it will be used. Otherwise, if |back| has a
  // canonical name, it will be in the first slot in the merged alias field.
  front.MergeDnsAliasesFrom(back);

  // Only expected to merge entries from same source.
  DCHECK_EQ(front.source(), back.source());

  if (front.has_ttl() && back.has_ttl()) {
    front.ttl_ = std::min(front.ttl(), back.ttl());
  } else if (back.has_ttl()) {
    front.ttl_ = back.ttl();
  }

  front.expires_ = std::min(front.expires(), back.expires());
  front.network_changes_ =
      std::max(front.network_changes(), back.network_changes());

  front.total_hits_ = front.total_hits_ + back.total_hits_;
  front.stale_hits_ = front.stale_hits_ + back.stale_hits_;

  return front;
}

HostCache::Entry HostCache::Entry::CopyWithDefaultPort(uint16_t port) const {
  Entry copy(*this);

  if (copy.ip_endpoints_) {
    for (IPEndPoint& endpoint : copy.ip_endpoints_.value()) {
      if (endpoint.port() == 0)
        endpoint = IPEndPoint(endpoint.address(), port);
    }
  }

  if (copy.legacy_addresses_) {
    for (IPEndPoint& endpoint : copy.legacy_addresses_.value().endpoints()) {
      if (endpoint.port() == 0)
        endpoint = IPEndPoint(endpoint.address(), port);
    }
  }

  if (copy.hostnames_) {
    for (HostPortPair& hostname : copy.hostnames_.value()) {
      if (hostname.port() == 0)
        hostname = HostPortPair(hostname.host(), port);
    }
  }

  return copy;
}

HostCache::Entry& HostCache::Entry::operator=(const Entry& entry) = default;

HostCache::Entry& HostCache::Entry::operator=(Entry&& entry) = default;

HostCache::Entry::Entry(const HostCache::Entry& entry,
                        base::TimeTicks now,
                        base::TimeDelta ttl,
                        int network_changes)
    : error_(entry.error()),
      ip_endpoints_(entry.ip_endpoints_),
      endpoint_metadatas_(entry.endpoint_metadatas_),
      aliases_(base::OptionalFromPtr(entry.aliases())),
      legacy_addresses_(entry.legacy_addresses()),
      text_records_(entry.text_records()),
      hostnames_(entry.hostnames()),
      experimental_results_(entry.experimental_results()),
      source_(entry.source()),
      pinning_(entry.pinning()),
      ttl_(entry.ttl()),
      expires_(now + ttl),
      network_changes_(network_changes) {}

HostCache::Entry::Entry(
    int error,
    absl::optional<std::vector<IPEndPoint>> ip_endpoints,
    absl::optional<
        std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>>
        endpoint_metadatas,
    absl::optional<std::set<std::string>> aliases,
    const absl::optional<AddressList>& legacy_addresses,
    absl::optional<std::vector<std::string>>&& text_records,
    absl::optional<std::vector<HostPortPair>>&& hostnames,
    absl::optional<std::vector<bool>>&& experimental_results,
    Source source,
    base::TimeTicks expires,
    int network_changes)
    : error_(error),
      ip_endpoints_(std::move(ip_endpoints)),
      endpoint_metadatas_(std::move(endpoint_metadatas)),
      aliases_(std::move(aliases)),
      legacy_addresses_(legacy_addresses),
      text_records_(std::move(text_records)),
      hostnames_(std::move(hostnames)),
      experimental_results_(std::move(experimental_results)),
      source_(source),
      expires_(expires),
      network_changes_(network_changes) {}

void HostCache::Entry::PrepareForCacheInsertion() {
  experimental_results_.reset();
}

bool HostCache::Entry::IsStale(base::TimeTicks now, int network_changes) const {
  EntryStaleness stale;
  stale.expired_by = now - expires_;
  stale.network_changes = network_changes - network_changes_;
  stale.stale_hits = stale_hits_;
  return stale.is_stale();
}

void HostCache::Entry::CountHit(bool hit_is_stale) {
  ++total_hits_;
  if (hit_is_stale)
    ++stale_hits_;
}

void HostCache::Entry::GetStaleness(base::TimeTicks now,
                                    int network_changes,
                                    EntryStaleness* out) const {
  DCHECK(out);
  out->expired_by = now - expires_;
  out->network_changes = network_changes - network_changes_;
  out->stale_hits = stale_hits_;
}

base::Value HostCache::Entry::NetLogParams() const {
  return GetAsValue(false /* include_staleness */);
}

void HostCache::Entry::MergeAddressesFrom(const HostCache::Entry& source) {
  MergeLists(&legacy_addresses_, source.legacy_addresses());
  if (!legacy_addresses_ || legacy_addresses_->size() <= 1)
    return;  // Nothing to do.

  legacy_addresses_->Deduplicate();

  std::stable_sort(legacy_addresses_->begin(), legacy_addresses_->end(),
                   [](const IPEndPoint& lhs, const IPEndPoint& rhs) {
                     // Return true iff |lhs < rhs|.
                     return lhs.GetFamily() == ADDRESS_FAMILY_IPV6 &&
                            rhs.GetFamily() == ADDRESS_FAMILY_IPV4;
                   });
}

void HostCache::Entry::MergeDnsAliasesFrom(const HostCache::Entry& source) {
  // No aliases to merge if source has no AddressList.
  if (!source.legacy_addresses())
    return;

  // We expect this to be true because the address merging should have already
  // created the AddressList if the source had one but the target didn't.
  DCHECK(legacy_addresses());

  // Nothing to merge.
  if (source.legacy_addresses()->dns_aliases().empty())
    return;

  // No aliases pre-existing in target, so simply set target's aliases to
  // source's. This takes care of the case where target does not have a usable
  // canonical name, but source does.
  if (legacy_addresses()->dns_aliases().empty()) {
    legacy_addresses_->SetDnsAliases(source.legacy_addresses()->dns_aliases());
    return;
  }

  DCHECK(legacy_addresses()->dns_aliases() != std::vector<std::string>({""}));
  DCHECK(source.legacy_addresses()->dns_aliases() !=
         std::vector<std::string>({""}));

  // We need to check for possible blanks and duplicates in the source's
  // aliases.
  std::unordered_set<std::string> aliases_seen;
  std::vector<std::string> deduplicated_source_aliases;

  aliases_seen.insert(legacy_addresses()->dns_aliases().begin(),
                      legacy_addresses()->dns_aliases().end());

  for (const auto& alias : source.legacy_addresses()->dns_aliases()) {
    if (alias != "" && aliases_seen.find(alias) == aliases_seen.end()) {
      aliases_seen.insert(alias);
      deduplicated_source_aliases.push_back(alias);
    }
  }

  // The first entry of target's aliases must remain in place,
  // as it's the canonical name, so we append source's aliases to the back.
  legacy_addresses_->AppendDnsAliases(std::move(deduplicated_source_aliases));
}

base::Value HostCache::Entry::GetAsValue(bool include_staleness) const {
  base::Value entry_dict(base::Value::Type::DICTIONARY);

  if (include_staleness) {
    // The kExpirationKey value is using TimeTicks instead of Time used if
    // |include_staleness| is false, so it cannot be used to deserialize.
    // This is ok as it is used only for netlog.
    entry_dict.SetStringKey(kExpirationKey,
                            NetLog::TickCountToString(expires()));
    entry_dict.SetIntKey(kTtlKey, ttl().InMilliseconds());
    entry_dict.SetIntKey(kNetworkChangesKey, network_changes());
    // The "pinned" status is meaningful only if "network_changes" is also
    // preserved.
    if (pinning())
      entry_dict.SetBoolKey(kPinnedKey, *pinning());
  } else {
    // Convert expiration time in TimeTicks to Time for serialization, using a
    // string because base::Value doesn't handle 64-bit integers.
    base::Time expiration_time =
        base::Time::Now() - (base::TimeTicks::Now() - expires());
    entry_dict.SetStringKey(
        kExpirationKey,
        base::NumberToString(expiration_time.ToInternalValue()));
  }

  if (error() != OK) {
    entry_dict.SetIntKey(kNetErrorKey, error());
  } else {
    if (ip_endpoints_) {
      base::Value::ListStorage ip_endpoints_list;
      for (const IPEndPoint& ip_endpoint : ip_endpoints_.value()) {
        ip_endpoints_list.push_back(IpEndpointToValue(ip_endpoint));
      }
      entry_dict.SetKey(kIpEndpointsKey,
                        base::Value(std::move(ip_endpoints_list)));
    }

    if (endpoint_metadatas_) {
      base::Value::ListStorage endpoint_metadatas_list;
      for (const auto& endpoint_metadata_pair : endpoint_metadatas_.value()) {
        endpoint_metadatas_list.push_back(
            EndpointMetadataPairToValue(endpoint_metadata_pair));
      }
      entry_dict.SetKey(kEndpointMetadatasKey,
                        base::Value(std::move(endpoint_metadatas_list)));
    }

    if (aliases()) {
      base::Value::ListStorage alias_list;
      for (const std::string& alias : *aliases()) {
        alias_list.emplace_back(alias);
      }
      entry_dict.SetKey(kAliasesKey, base::Value(std::move(alias_list)));
    }

    if (legacy_addresses()) {
      // Append all of the resolved addresses.
      base::ListValue addresses_value;
      for (const IPEndPoint& address : legacy_addresses().value()) {
        addresses_value.Append(address.ToStringWithoutPort());
      }
      entry_dict.SetKey(kAddressesKey, std::move(addresses_value));
    }

    if (text_records()) {
      // Append all resolved text records.
      base::ListValue text_list_value;
      for (const std::string& text_record : text_records().value()) {
        text_list_value.Append(text_record);
      }
      entry_dict.SetKey(kTextRecordsKey, std::move(text_list_value));
    }

    if (hostnames()) {
      // Append all the resolved hostnames.
      base::ListValue hostnames_value;
      base::ListValue host_ports_value;
      for (const HostPortPair& hostname : hostnames().value()) {
        hostnames_value.Append(hostname.host());
        host_ports_value.Append(hostname.port());
      }
      entry_dict.SetKey(kHostnameResultsKey, std::move(hostnames_value));
      entry_dict.SetKey(kHostPortsKey, std::move(host_ports_value));
    }
  }

  return entry_dict;
}

// static
const HostCache::EntryStaleness HostCache::kNotStale = {base::Seconds(-1), 0,
                                                        0};

HostCache::HostCache(size_t max_entries)
    : max_entries_(max_entries),
      network_changes_(0),
      restore_size_(0),
      delegate_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

HostCache::~HostCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

const std::pair<const HostCache::Key, HostCache::Entry>*
HostCache::Lookup(const Key& key, base::TimeTicks now, bool ignore_secure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return nullptr;

  auto* result = LookupInternalIgnoringFields(key, now, ignore_secure);
  if (!result)
    return nullptr;

  auto* entry = &result->second;
  if (entry->IsStale(now, network_changes_))
    return nullptr;

  entry->CountHit(/* hit_is_stale= */ false);
  return result;
}

const std::pair<const HostCache::Key, HostCache::Entry>* HostCache::LookupStale(
    const Key& key,
    base::TimeTicks now,
    HostCache::EntryStaleness* stale_out,
    bool ignore_secure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return nullptr;

  auto* result = LookupInternalIgnoringFields(key, now, ignore_secure);
  if (!result)
    return nullptr;

  auto* entry = &result->second;
  bool is_stale = entry->IsStale(now, network_changes_);
  entry->CountHit(/* hit_is_stale= */ is_stale);

  if (stale_out)
    entry->GetStaleness(now, network_changes_, stale_out);
  return result;
}

// static
std::pair<const HostCache::Key, HostCache::Entry>*
HostCache::GetLessStaleMoreSecureResult(
    base::TimeTicks now,
    std::pair<const HostCache::Key, HostCache::Entry>* result1,
    std::pair<const HostCache::Key, HostCache::Entry>* result2) {
  // Prefer a non-null result if possible.
  if (!result1 && !result2)
    return nullptr;
  if (result1 && !result2)
    return result1;
  if (!result1 && result2)
    return result2;

  // Both result1 are result2 are non-null.
  EntryStaleness staleness1, staleness2;
  result1->second.GetStaleness(now, 0, &staleness1);
  result2->second.GetStaleness(now, 0, &staleness2);
  if (staleness1.network_changes == staleness2.network_changes) {
    // Exactly one of the results should be secure.
    DCHECK(result1->first.secure != result2->first.secure);
    // If the results have the same number of network changes, prefer a
    // non-expired result.
    if (staleness1.expired_by.is_negative() &&
        staleness2.expired_by >= base::TimeDelta()) {
      return result1;
    }
    if (staleness1.expired_by >= base::TimeDelta() &&
        staleness2.expired_by.is_negative()) {
      return result2;
    }
    // Both results are equally stale, so prefer a secure result.
    return (result1->first.secure) ? result1 : result2;
  }
  // Prefer the result with the fewest network changes.
  return (staleness1.network_changes < staleness2.network_changes) ? result1
                                                                   : result2;
}

std::pair<const HostCache::Key, HostCache::Entry>*
HostCache::LookupInternalIgnoringFields(const Key& initial_key,
                                        base::TimeTicks now,
                                        bool ignore_secure) {
  std::pair<const HostCache::Key, HostCache::Entry>* preferred_result =
      LookupInternal(initial_key);

  if (ignore_secure) {
    Key effective_key = initial_key;
    effective_key.secure = !initial_key.secure;
    preferred_result = GetLessStaleMoreSecureResult(
        now, preferred_result, LookupInternal(effective_key));
  }

  return preferred_result;
}

std::pair<const HostCache::Key, HostCache::Entry>* HostCache::LookupInternal(
    const Key& key) {
  auto it = entries_.find(key);
  return (it != entries_.end()) ? &*it : nullptr;
}

void HostCache::Set(const Key& key,
                    const Entry& entry,
                    base::TimeTicks now,
                    base::TimeDelta ttl) {
  TRACE_EVENT0(NetTracingCategory(), "HostCache::Set");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return;

  bool has_active_pin = false;
  bool result_changed = false;
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    has_active_pin = HasActivePin(it->second);

    // TODO(juliatuttle): Remember some old metadata (hit count or frequency or
    // something like that) if it's useful for better eviction algorithms?
    result_changed = entry.error() == OK && !it->second.ContentsEqual(entry);
    entries_.erase(it);
  } else {
    result_changed = true;
    // This loop almost always runs at most once, for total runtime
    // O(max_entries_).  It only runs more than once if the cache was over-full
    // due to pinned entries, and this is the first call to Set() after
    // Invalidate().  The amortized cost remains O(size()) per call to Set().
    while (size() >= max_entries_ && EvictOneEntry(now)) {
    }
  }

  Entry entry_for_cache(entry, now, ttl, network_changes_);
  entry_for_cache.set_pinning(entry.pinning().value_or(has_active_pin));
  entry_for_cache.PrepareForCacheInsertion();
  AddEntry(key, std::move(entry_for_cache));

  if (delegate_ && result_changed)
    delegate_->ScheduleWrite();
}

const HostCache::Key* HostCache::GetMatchingKeyForTesting(
    base::StringPiece hostname,
    HostCache::Entry::Source* source_out,
    HostCache::EntryStaleness* stale_out) const {
  for (const EntryMap::value_type& entry : entries_) {
    if (GetHostname(entry.first.host) == hostname) {
      if (source_out != nullptr)
        *source_out = entry.second.source();
      if (stale_out != nullptr) {
        entry.second.GetStaleness(tick_clock_->NowTicks(), network_changes_,
                                  stale_out);
      }
      return &entry.first;
    }
  }

  return nullptr;
}

void HostCache::AddEntry(const Key& key, Entry&& entry) {
  DCHECK_EQ(0u, entries_.count(key));
  DCHECK(entry.pinning().has_value());
  entries_.emplace(key, std::move(entry));
}

void HostCache::Invalidate() {
  ++network_changes_;
}

void HostCache::set_persistence_delegate(PersistenceDelegate* delegate) {
  // A PersistenceDelegate shouldn't be added if there already was one, and
  // shouldn't be removed (by setting to nullptr) if it wasn't previously there.
  DCHECK_NE(delegate == nullptr, delegate_ == nullptr);
  delegate_ = delegate;
}

void HostCache::clear() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Don't bother scheduling a write if there's nothing to clear.
  if (size() == 0)
    return;

  entries_.clear();
  if (delegate_)
    delegate_->ScheduleWrite();
}

void HostCache::ClearForHosts(
    const base::RepeatingCallback<bool(const std::string&)>& host_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (host_filter.is_null()) {
    clear();
    return;
  }

  bool changed = false;
  for (auto it = entries_.begin(); it != entries_.end();) {
    auto next_it = std::next(it);

    if (host_filter.Run(GetHostname(it->first.host))) {
      entries_.erase(it);
      changed = true;
    }

    it = next_it;
  }

  if (delegate_ && changed)
    delegate_->ScheduleWrite();
}

void HostCache::GetList(base::Value* entry_list,
                        bool include_staleness,
                        SerializationType serialization_type) const {
  DCHECK(entry_list);
  DCHECK(entry_list->is_list());
  entry_list->ClearList();

  for (const auto& pair : entries_) {
    const Key& key = pair.first;
    const Entry& entry = pair.second;

    base::Value network_isolation_key_value;
    if (serialization_type == SerializationType::kRestorable) {
      // Don't save entries associated with ephemeral NetworkIsolationKeys.
      if (!key.network_isolation_key.ToValue(&network_isolation_key_value))
        continue;
    } else {
      // ToValue() fails for transient NIKs, since they should never be
      // serialized to disk in a restorable format, so use ToDebugString() when
      // serializing for debugging instead of for restoring from disk.
      network_isolation_key_value =
          base::Value(key.network_isolation_key.ToDebugString());
    }

    auto entry_dict =
        std::make_unique<base::Value>(entry.GetAsValue(include_staleness));

    const auto* host = absl::get_if<url::SchemeHostPort>(&key.host);
    if (host) {
      entry_dict->SetStringKey(kSchemeKey, host->scheme());
      entry_dict->SetStringKey(kHostnameKey, host->host());
      entry_dict->SetIntKey(kPortKey, host->port());
    } else {
      entry_dict->SetStringKey(kHostnameKey, absl::get<std::string>(key.host));
    }

    entry_dict->SetIntKey(kDnsQueryTypeKey,
                          base::strict_cast<int>(key.dns_query_type));
    entry_dict->SetIntKey(kFlagsKey, key.host_resolver_flags);
    entry_dict->SetIntKey(kHostResolverSourceKey,
                          static_cast<int>(key.host_resolver_source));
    entry_dict->SetKey(kNetworkIsolationKeyKey,
                       std::move(network_isolation_key_value));
    entry_dict->SetBoolKey(kSecureKey, static_cast<bool>(key.secure));

    entry_list->Append(std::move(*entry_dict));
  }
}

bool HostCache::RestoreFromListValue(const base::Value& old_cache) {
  // Reset the restore size to 0.
  restore_size_ = 0;

  for (const auto& entry_dict : old_cache.GetListDeprecated()) {
    // If the cache is already full, don't bother prioritizing what to evict,
    // just stop restoring.
    if (size() == max_entries_)
      break;

    if (!entry_dict.is_dict())
      return false;

    const std::string* hostname_ptr = entry_dict.FindStringKey(kHostnameKey);
    if (!hostname_ptr || !IsValidHostname(*hostname_ptr)) {
      return false;
    }

    // Use presence of scheme to determine host type.
    const std::string* scheme_ptr = entry_dict.FindStringKey(kSchemeKey);
    absl::variant<url::SchemeHostPort, std::string> host;
    if (scheme_ptr) {
      absl::optional<int> port = entry_dict.FindIntKey(kPortKey);
      if (!port || !base::IsValueInRangeForNumericType<uint16_t>(port.value()))
        return false;

      url::SchemeHostPort scheme_host_port(*scheme_ptr, *hostname_ptr,
                                           port.value());
      if (!scheme_host_port.IsValid())
        return false;
      host = std::move(scheme_host_port);
    } else {
      host = *hostname_ptr;
    }

    const std::string* expiration_ptr =
        entry_dict.FindStringKey(kExpirationKey);
    absl::optional<int> maybe_flags = entry_dict.FindIntKey(kFlagsKey);
    if (expiration_ptr == nullptr || !maybe_flags.has_value())
      return false;
    std::string expiration(*expiration_ptr);
    HostResolverFlags flags = maybe_flags.value();

    absl::optional<int> maybe_dns_query_type =
        entry_dict.FindIntKey(kDnsQueryTypeKey);
    if (!maybe_dns_query_type.has_value())
      return false;
    DnsQueryType dns_query_type =
        static_cast<DnsQueryType>(maybe_dns_query_type.value());

    // HostResolverSource is optional.
    int host_resolver_source =
        entry_dict.FindIntKey(kHostResolverSourceKey)
            .value_or(static_cast<int>(HostResolverSource::ANY));

    const base::Value* network_isolation_key_value =
        entry_dict.FindKey(kNetworkIsolationKeyKey);
    NetworkIsolationKey network_isolation_key;
    if (!network_isolation_key_value ||
        network_isolation_key_value->type() == base::Value::Type::STRING ||
        !NetworkIsolationKey::FromValue(*network_isolation_key_value,
                                        &network_isolation_key)) {
      return false;
    }

    bool secure = entry_dict.FindBoolKey(kSecureKey).value_or(false);

    int error = OK;
    const base::Value* ip_endpoints_value = nullptr;
    const base::Value* endpoint_metadatas_value = nullptr;
    const base::Value* aliases_value = nullptr;
    const base::Value* legacy_addresses_value = nullptr;
    const base::Value* text_records_value = nullptr;
    const base::Value* hostname_records_value = nullptr;
    const base::Value* host_ports_value = nullptr;
    absl::optional<int> maybe_error = entry_dict.FindIntKey(kNetErrorKey);
    absl::optional<bool> maybe_pinned = entry_dict.FindBoolKey(kPinnedKey);
    if (maybe_error.has_value()) {
      error = maybe_error.value();
    } else {
      ip_endpoints_value = entry_dict.FindListKey(kIpEndpointsKey);
      endpoint_metadatas_value = entry_dict.FindListKey(kEndpointMetadatasKey);
      aliases_value = entry_dict.FindListKey(kAliasesKey);
      legacy_addresses_value = entry_dict.FindListKey(kAddressesKey);
      text_records_value = entry_dict.FindListKey(kTextRecordsKey);
      hostname_records_value = entry_dict.FindListKey(kHostnameResultsKey);
      host_ports_value = entry_dict.FindListKey(kHostPortsKey);

      if ((hostname_records_value == nullptr && host_ports_value != nullptr) ||
          (hostname_records_value != nullptr && host_ports_value == nullptr)) {
        return false;
      }
    }

    int64_t time_internal;
    if (!base::StringToInt64(expiration, &time_internal))
      return false;

    base::TimeTicks expiration_time =
        tick_clock_->NowTicks() -
        (base::Time::Now() - base::Time::FromInternalValue(time_internal));

    absl::optional<std::vector<IPEndPoint>> ip_endpoints;
    if (ip_endpoints_value) {
      ip_endpoints.emplace();
      for (const base::Value& ip_endpoint_value :
           ip_endpoints_value->GetListDeprecated()) {
        absl::optional<IPEndPoint> ip_endpoint =
            IpEndpointFromValue(ip_endpoint_value);
        if (!ip_endpoint)
          return false;
        ip_endpoints->push_back(std::move(ip_endpoint).value());
      }
    }

    absl::optional<
        std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>>
        endpoint_metadatas;
    if (endpoint_metadatas_value) {
      endpoint_metadatas.emplace();
      for (const base::Value& endpoint_metadata_value :
           endpoint_metadatas_value->GetListDeprecated()) {
        absl::optional<
            std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>>
            pair = EndpointMetadataPairFromValue(endpoint_metadata_value);
        if (!pair)
          return false;
        endpoint_metadatas->insert(std::move(pair).value());
      }
    }

    absl::optional<std::set<std::string>> aliases;
    if (aliases_value) {
      aliases.emplace();
      for (const base::Value& alias_value :
           aliases_value->GetListDeprecated()) {
        if (!alias_value.is_string())
          return false;
        aliases->insert(alias_value.GetString());
      }
    }

    absl::optional<AddressList> legacy_address_list;
    if (!AddressListFromListValue(legacy_addresses_value,
                                  &legacy_address_list)) {
      return false;
    }

    absl::optional<std::vector<std::string>> text_records;
    if (text_records_value) {
      text_records.emplace();
      for (const base::Value& value : text_records_value->GetListDeprecated()) {
        if (!value.is_string())
          return false;
        text_records.value().push_back(value.GetString());
      }
    }

    absl::optional<std::vector<HostPortPair>> hostname_records;
    if (hostname_records_value) {
      DCHECK(host_ports_value);
      if (hostname_records_value->GetListDeprecated().size() !=
          host_ports_value->GetListDeprecated().size()) {
        return false;
      }

      hostname_records.emplace();
      for (size_t i = 0; i < hostname_records_value->GetListDeprecated().size();
           ++i) {
        if (!hostname_records_value->GetListDeprecated()[i].is_string() ||
            !host_ports_value->GetListDeprecated()[i].is_int() ||
            !base::IsValueInRangeForNumericType<uint16_t>(
                host_ports_value->GetListDeprecated()[i].GetInt())) {
          return false;
        }
        hostname_records.value().push_back(HostPortPair(
            hostname_records_value->GetListDeprecated()[i].GetString(),
            base::checked_cast<uint16_t>(
                host_ports_value->GetListDeprecated()[i].GetInt())));
      }
    }

    // We do not intend to serialize experimental results with the host cache.
    absl::optional<std::vector<bool>> experimental_results;

    // Assume an empty address list if we have an address type and no results.
    if (IsAddressType(dns_query_type) && !ip_endpoints &&
        !legacy_address_list && !text_records && !hostname_records) {
      legacy_address_list.emplace();
    }

    Key key(std::move(host), dns_query_type, flags,
            static_cast<HostResolverSource>(host_resolver_source),
            network_isolation_key);
    key.secure = secure;

    // If the key is already in the cache, assume it's more recent and don't
    // replace the entry.
    auto found = entries_.find(key);
    if (found == entries_.end()) {
      Entry entry(error, std::move(ip_endpoints), std::move(endpoint_metadatas),
                  std::move(aliases), legacy_address_list,
                  std::move(text_records), std::move(hostname_records),
                  std::move(experimental_results), Entry::SOURCE_UNKNOWN,
                  expiration_time, network_changes_ - 1);
      entry.set_pinning(maybe_pinned.value_or(false));
      AddEntry(key, std::move(entry));
      restore_size_++;
    }
  }
  return true;
}

size_t HostCache::size() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return entries_.size();
}

size_t HostCache::max_entries() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return max_entries_;
}

// static
std::unique_ptr<HostCache> HostCache::CreateDefaultCache() {
#if defined(ENABLE_BUILT_IN_DNS)
  const size_t kDefaultMaxEntries = 1000;
#else
  const size_t kDefaultMaxEntries = 100;
#endif
  return std::make_unique<HostCache>(kDefaultMaxEntries);
}

bool HostCache::EvictOneEntry(base::TimeTicks now) {
  DCHECK_LT(0u, entries_.size());

  absl::optional<net::HostCache::EntryMap::iterator> oldest_it;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    const Entry& entry = it->second;
    if (HasActivePin(entry)) {
      continue;
    }

    if (!oldest_it) {
      oldest_it = it;
      continue;
    }

    const Entry& oldest = (*oldest_it)->second;
    if ((entry.expires() < oldest.expires()) &&
        (entry.IsStale(now, network_changes_) ||
         !oldest.IsStale(now, network_changes_))) {
      oldest_it = it;
    }
  }

  if (oldest_it) {
    entries_.erase(*oldest_it);
    return true;
  }
  return false;
}

bool HostCache::HasActivePin(const Entry& entry) {
  return entry.pinning().value_or(false) &&
         entry.network_changes() == network_changes();
}

}  // namespace net

// Debug logging support
std::ostream& operator<<(std::ostream& out,
                         const net::HostCache::EntryStaleness& s) {
  return out << "EntryStaleness{" << s.expired_by << ", " << s.network_changes
             << ", " << s.stale_hits << "}";
}
