// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_cache_impl.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/stl_util.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "net/base/url_util.h"
#include "net/log/net_log.h"

namespace net {

ReportingCacheImpl::ReportingCacheImpl(ReportingContext* context)
    : context_(context) {
  DCHECK(context_);
}

ReportingCacheImpl::~ReportingCacheImpl() {
  for (const auto& report : reports_) {
    if (report->status != ReportingReport::Status::DOOMED)
      report->outcome = ReportingReport::Outcome::ERASED_REPORTING_SHUT_DOWN;
  }
}

void ReportingCacheImpl::AddReport(
    const NetworkIsolationKey& network_isolation_key,
    const GURL& url,
    const std::string& user_agent,
    const std::string& group_name,
    const std::string& type,
    std::unique_ptr<const base::Value> body,
    int depth,
    base::TimeTicks queued,
    int attempts) {
  auto report = std::make_unique<ReportingReport>(
      network_isolation_key, url, user_agent, group_name, type, std::move(body),
      depth, queued, attempts);

  auto inserted = reports_.insert(std::move(report));
  DCHECK(inserted.second);

  if (reports_.size() > context_->policy().max_report_count) {
    // There should be at most one extra report (the one added above).
    DCHECK_EQ(context_->policy().max_report_count + 1, reports_.size());
    ReportSet::const_iterator to_evict = FindReportToEvict();
    DCHECK(to_evict != reports_.end());
    // The newly-added report isn't pending, so even if all other reports are
    // pending, the cache should have a report to evict.
    DCHECK(!to_evict->get()->IsUploadPending());
    to_evict->get()->outcome = ReportingReport::Outcome::ERASED_EVICTED;
    reports_.erase(to_evict);
  }

  context_->NotifyCachedReportsUpdated();
}

void ReportingCacheImpl::GetReports(
    std::vector<const ReportingReport*>* reports_out) const {
  reports_out->clear();
  for (const auto& report : reports_) {
    if (report->status != ReportingReport::Status::DOOMED)
      reports_out->push_back(report.get());
  }
}

base::Value ReportingCacheImpl::GetReportsAsValue() const {
  // Sort all unsent reports by origin and timestamp.
  std::vector<const ReportingReport*> sorted_reports;
  sorted_reports.reserve(reports_.size());
  for (const auto& report : reports_) {
    sorted_reports.push_back(report.get());
  }
  std::sort(sorted_reports.begin(), sorted_reports.end(),
            [](const ReportingReport* report1, const ReportingReport* report2) {
              return std::tie(report1->queued, report1->url) <
                     std::tie(report2->queued, report2->url);
            });

  std::vector<base::Value> report_list;
  for (const ReportingReport* report : sorted_reports) {
    base::Value report_dict(base::Value::Type::DICTIONARY);
    report_dict.SetKey(
        "network_isolation_key",
        base::Value(report->network_isolation_key.ToDebugString()));
    report_dict.SetKey("url", base::Value(report->url.spec()));
    report_dict.SetKey("group", base::Value(report->group));
    report_dict.SetKey("type", base::Value(report->type));
    report_dict.SetKey("depth", base::Value(report->depth));
    report_dict.SetKey("queued",
                       base::Value(NetLog::TickCountToString(report->queued)));
    report_dict.SetKey("attempts", base::Value(report->attempts));
    if (report->body) {
      report_dict.SetKey("body", report->body->Clone());
    }
    switch (report->status) {
      case ReportingReport::Status::DOOMED:
        report_dict.SetKey("status", base::Value("doomed"));
        break;
      case ReportingReport::Status::PENDING:
        report_dict.SetKey("status", base::Value("pending"));
        break;
      case ReportingReport::Status::QUEUED:
        report_dict.SetKey("status", base::Value("queued"));
        break;
    }
    report_list.push_back(std::move(report_dict));
  }
  return base::Value(std::move(report_list));
}

std::vector<const ReportingReport*> ReportingCacheImpl::GetReportsToDeliver() {
  std::vector<const ReportingReport*> reports_out;
  for (const auto& report : reports_) {
    if (report->IsUploadPending())
      continue;
    report->status = ReportingReport::Status::PENDING;
    reports_out.push_back(report.get());
  }
  return reports_out;
}

void ReportingCacheImpl::ClearReportsPending(
    const std::vector<const ReportingReport*>& reports) {
  std::vector<const ReportingReport*> reports_to_remove;

  for (const ReportingReport* report : reports) {
    auto it = reports_.find(report);
    DCHECK(it != reports_.end());
    if (it->get()->status == ReportingReport::Status::DOOMED) {
      reports_.erase(it);
    } else {
      DCHECK_EQ(ReportingReport::Status::PENDING, it->get()->status);
      it->get()->status = ReportingReport::Status::QUEUED;
    }
  }
}

void ReportingCacheImpl::IncrementReportsAttempts(
    const std::vector<const ReportingReport*>& reports) {
  for (const ReportingReport* report : reports) {
    auto it = reports_.find(report);
    DCHECK(it != reports_.end());
    it->get()->attempts++;
  }

  context_->NotifyCachedReportsUpdated();
}

void ReportingCacheImpl::IncrementEndpointDeliveries(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url,
    int reports_delivered,
    bool successful) {
  EndpointMap::iterator endpoint_it = FindEndpointIt(group_key, url);
  // The endpoint may have been removed while the upload was in progress. In
  // that case, we no longer care about the stats for the removed endpoint.
  if (endpoint_it == endpoints_.end())
    return;

  ReportingEndpoint::Statistics& stats = endpoint_it->second.stats;
  ++stats.attempted_uploads;
  stats.attempted_reports += reports_delivered;
  if (successful) {
    ++stats.successful_uploads;
    stats.successful_reports += reports_delivered;
  }
}

void ReportingCacheImpl::RemoveReports(
    const std::vector<const ReportingReport*>& reports,
    ReportingReport::Outcome outcome) {
  base::Optional<base::TimeTicks> delivered = base::nullopt;
  if (outcome == ReportingReport::Outcome::DELIVERED)
    delivered = tick_clock().NowTicks();
  for (const ReportingReport* report : reports) {
    auto it = reports_.find(report);
    DCHECK(it != reports_.end());

    it->get()->outcome = outcome;
    it->get()->delivered = delivered;

    if (it->get()->IsUploadPending()) {
      it->get()->status = ReportingReport::Status::DOOMED;
    } else {
      reports_.erase(it);
    }
  }
  context_->NotifyCachedReportsUpdated();
}

void ReportingCacheImpl::RemoveAllReports(ReportingReport::Outcome outcome) {
  std::vector<const ReportingReport*> reports_to_remove;
  GetReports(&reports_to_remove);
  RemoveReports(reports_to_remove, outcome);
}

size_t ReportingCacheImpl::GetFullReportCountForTesting() const {
  return reports_.size();
}

bool ReportingCacheImpl::IsReportPendingForTesting(
    const ReportingReport* report) const {
  DCHECK(report);
  DCHECK(reports_.find(report) != reports_.end());
  return report->IsUploadPending();
}

bool ReportingCacheImpl::IsReportDoomedForTesting(
    const ReportingReport* report) const {
  DCHECK(report);
  DCHECK(reports_.find(report) != reports_.end());
  return report->status == ReportingReport::Status::DOOMED;
}

void ReportingCacheImpl::OnParsedHeader(
    const NetworkIsolationKey& network_isolation_key,
    const url::Origin& origin,
    std::vector<ReportingEndpointGroup> parsed_header) {
  SanityCheckClients();

  Client new_client(network_isolation_key, origin);
  base::Time now = clock().Now();
  new_client.last_used = now;

  std::map<ReportingEndpointGroupKey, std::set<GURL>> endpoints_per_group;

  for (const auto& parsed_endpoint_group : parsed_header) {
    new_client.endpoint_group_names.insert(
        parsed_endpoint_group.group_key.group_name);

    // Creates an endpoint group and sets its |last_used| to |now|.
    CachedReportingEndpointGroup new_group(parsed_endpoint_group, now);

    // Consistency check: the new client should have the same NIK and origin as
    // all groups parsed from this header.
    DCHECK_EQ(new_group.group_key.network_isolation_key,
              new_client.network_isolation_key);
    DCHECK_EQ(new_group.group_key.origin, new_client.origin);

    std::set<GURL> new_endpoints;
    for (const auto& parsed_endpoint_info : parsed_endpoint_group.endpoints) {
      new_endpoints.insert(parsed_endpoint_info.url);
      endpoints_per_group[new_group.group_key].insert(parsed_endpoint_info.url);
      ReportingEndpoint new_endpoint(new_group.group_key,
                                     std::move(parsed_endpoint_info));
      AddOrUpdateEndpoint(std::move(new_endpoint));
    }

    // Remove endpoints that may have been previously configured for this group,
    // but which were not specified in the current header.
    RemoveEndpointsInGroupOtherThan(new_group.group_key, new_endpoints);

    AddOrUpdateEndpointGroup(std::move(new_group));
  }

  // Compute the total endpoint count for this origin. We can't just count the
  // number of endpoints per group because there may be duplicate endpoint URLs,
  // which we ignore. See http://crbug.com/983000 for discussion.
  // TODO(crbug.com/983000): Allow duplicate endpoint URLs.
  for (const auto& group_key_and_endpoint_set : endpoints_per_group) {
    new_client.endpoint_count += group_key_and_endpoint_set.second.size();
  }

  // Remove endpoint groups that may have been configured for an existing client
  // for |origin|, but which are not specified in the current header.
  RemoveEndpointGroupsForClientOtherThan(network_isolation_key, origin,
                                         new_client.endpoint_group_names);

  EnforcePerClientAndGlobalEndpointLimits(
      AddOrUpdateClient(std::move(new_client)));
  SanityCheckClients();

  context_->NotifyCachedClientsUpdated();
}

std::set<url::Origin> ReportingCacheImpl::GetAllOrigins() const {
  SanityCheckClients();
  std::set<url::Origin> origins_out;
  for (const auto& domain_and_client : clients_) {
    origins_out.insert(domain_and_client.second.origin);
  }
  return origins_out;
}

void ReportingCacheImpl::RemoveClient(
    const NetworkIsolationKey& network_isolation_key,
    const url::Origin& origin) {
  SanityCheckClients();
  ClientMap::iterator client_it = FindClientIt(network_isolation_key, origin);
  if (client_it == clients_.end())
    return;
  RemoveClientInternal(client_it);
  SanityCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveClientsForOrigin(const url::Origin& origin) {
  SanityCheckClients();
  std::string domain = origin.host();
  const auto domain_range = clients_.equal_range(domain);
  ClientMap::iterator it = domain_range.first;
  while (it != domain_range.second) {
    if (it->second.origin == origin) {
      it = RemoveClientInternal(it);
      continue;
    }
    ++it;
  }
  SanityCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveAllClients() {
  SanityCheckClients();

  auto remove_it = clients_.begin();
  while (remove_it != clients_.end()) {
    remove_it = RemoveClientInternal(remove_it);
  }

  DCHECK(clients_.empty());
  DCHECK(endpoint_groups_.empty());
  DCHECK(endpoints_.empty());
  DCHECK(endpoint_its_by_url_.empty());

  SanityCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveEndpointGroup(
    const ReportingEndpointGroupKey& group_key) {
  SanityCheckClients();
  EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
  if (group_it == endpoint_groups_.end())
    return;
  ClientMap::iterator client_it = FindClientIt(group_key);
  DCHECK(client_it != clients_.end());

  RemoveEndpointGroupInternal(client_it, group_it);
  SanityCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveEndpointsForUrl(const GURL& url) {
  SanityCheckClients();

  auto url_range = endpoint_its_by_url_.equal_range(url);
  if (url_range.first == url_range.second)
    return;

  // Make a copy of the EndpointMap::iterators matching |url|, to avoid deleting
  // while iterating
  std::vector<EndpointMap::iterator> endpoint_its_to_remove;
  for (auto index_it = url_range.first; index_it != url_range.second;
       ++index_it) {
    endpoint_its_to_remove.push_back(index_it->second);
  }
  DCHECK_GT(endpoint_its_to_remove.size(), 0u);

  // Delete from the index, since we have the |url_range| already. This saves
  // us from having to remove them one by one, which would involve
  // iterating over the |url_range| on each call to RemoveEndpointInternal().
  endpoint_its_by_url_.erase(url_range.first, url_range.second);

  for (EndpointMap::iterator endpoint_it : endpoint_its_to_remove) {
    DCHECK(endpoint_it->second.info.url == url);
    const ReportingEndpointGroupKey& group_key = endpoint_it->first;
    ClientMap::iterator client_it = FindClientIt(group_key);
    DCHECK(client_it != clients_.end());
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
    DCHECK(group_it != endpoint_groups_.end());
    RemoveEndpointInternal(client_it, group_it, endpoint_it);
  }

  SanityCheckClients();
  context_->NotifyCachedClientsUpdated();
}

// Reconstruct an Client from the loaded endpoint groups, and add the
// loaded endpoints and endpoint groups into the cache.
void ReportingCacheImpl::AddClientsLoadedFromStore(
    std::vector<ReportingEndpoint> loaded_endpoints,
    std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups) {
  DCHECK(context_->IsClientDataPersisted());

  std::sort(loaded_endpoints.begin(), loaded_endpoints.end(),
            [](const ReportingEndpoint& a, const ReportingEndpoint& b) -> bool {
              return a.group_key < b.group_key;
            });
  std::sort(loaded_endpoint_groups.begin(), loaded_endpoint_groups.end(),
            [](const CachedReportingEndpointGroup& a,
               const CachedReportingEndpointGroup& b) -> bool {
              return a.group_key < b.group_key;
            });

  // If using a persistent store, cache should be empty before loading finishes.
  DCHECK(clients_.empty());
  DCHECK(endpoint_groups_.empty());
  DCHECK(endpoints_.empty());
  DCHECK(endpoint_its_by_url_.empty());

  // |loaded_endpoints| and |loaded_endpoint_groups| should both be sorted by
  // origin and group name.
  auto endpoints_it = loaded_endpoints.begin();
  auto endpoint_groups_it = loaded_endpoint_groups.begin();

  base::Optional<Client> client;

  while (endpoint_groups_it != loaded_endpoint_groups.end() &&
         endpoints_it != loaded_endpoints.end()) {
    const CachedReportingEndpointGroup& group = *endpoint_groups_it;
    const ReportingEndpointGroupKey& group_key = group.group_key;

    // These things should probably never happen:
    if (group_key < endpoints_it->group_key) {
      // This endpoint group has no associated endpoints, so move on to the next
      // endpoint group.
      ++endpoint_groups_it;
      continue;
    } else if (group_key > endpoints_it->group_key) {
      // This endpoint has no associated endpoint group, so move on to the next
      // endpoint.
      ++endpoints_it;
      continue;
    }

    DCHECK(group_key == endpoints_it->group_key);

    size_t cur_group_endpoints_count = 0;

    // Insert the endpoints corresponding to this group.
    while (endpoints_it != loaded_endpoints.end() &&
           endpoints_it->group_key == group_key) {
      EndpointMap::iterator inserted = endpoints_.insert(
          std::make_pair(group_key, std::move(*endpoints_it)));
      endpoint_its_by_url_.insert(
          std::make_pair(inserted->second.info.url, inserted));
      ++cur_group_endpoints_count;
      ++endpoints_it;
    }

    if (!client ||
        client->network_isolation_key != group_key.network_isolation_key ||
        client->origin != group_key.origin) {
      // Store the old client and start a new one.
      if (client) {
        ClientMap::iterator client_it = clients_.insert(
            std::make_pair(client->origin.host(), std::move(*client)));
        EnforcePerClientAndGlobalEndpointLimits(client_it);
      }
      DCHECK(FindClientIt(group_key) == clients_.end());
      client = base::make_optional(
          Client(group_key.network_isolation_key, group_key.origin));
    }
    DCHECK(client.has_value());
    client->endpoint_group_names.insert(group_key.group_name);
    client->endpoint_count += cur_group_endpoints_count;
    client->last_used = std::max(client->last_used, group.last_used);

    endpoint_groups_.insert(std::make_pair(group_key, std::move(group)));

    ++endpoint_groups_it;
  }

  if (client) {
    DCHECK(FindClientIt(client->network_isolation_key, client->origin) ==
           clients_.end());
    ClientMap::iterator client_it = clients_.insert(
        std::make_pair(client->origin.host(), std::move(*client)));
    EnforcePerClientAndGlobalEndpointLimits(client_it);
  }

  SanityCheckClients();
}

std::vector<ReportingEndpoint>
ReportingCacheImpl::GetCandidateEndpointsForDelivery(
    const ReportingEndpointGroupKey& group_key) {
  base::Time now = clock().Now();
  SanityCheckClients();

  // Look for an exact origin match for |origin| and |group|.
  EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
  if (group_it != endpoint_groups_.end() && group_it->second.expires > now) {
    ClientMap::iterator client_it = FindClientIt(group_key);
    MarkEndpointGroupAndClientUsed(client_it, group_it, now);
    SanityCheckClients();
    context_->NotifyCachedClientsUpdated();
    return GetEndpointsInGroup(group_it->first);
  }

  // If no endpoints were found for an exact match, look for superdomain matches
  // TODO(chlily): Limit the number of labels to go through when looking for a
  // superdomain match.
  std::string domain = group_key.origin.host();
  while (!domain.empty()) {
    const auto domain_range = clients_.equal_range(domain);
    for (auto client_it = domain_range.first; client_it != domain_range.second;
         ++client_it) {
      // Client for a superdomain of |origin|
      const Client& client = client_it->second;
      if (client.network_isolation_key != group_key.network_isolation_key)
        continue;
      ReportingEndpointGroupKey new_group(group_key.network_isolation_key,
                                          client.origin, group_key.group_name);
      group_it = FindEndpointGroupIt(new_group);

      if (group_it == endpoint_groups_.end())
        continue;

      const CachedReportingEndpointGroup& endpoint_group = group_it->second;
      // Check if the group is valid (unexpired and includes subdomains).
      if (endpoint_group.include_subdomains == OriginSubdomains::INCLUDE &&
          endpoint_group.expires > now) {
        MarkEndpointGroupAndClientUsed(client_it, group_it, now);
        SanityCheckClients();
        context_->NotifyCachedClientsUpdated();
        return GetEndpointsInGroup(new_group);
      }
    }
    domain = GetSuperdomain(domain);
  }
  return std::vector<ReportingEndpoint>();
}

base::Value ReportingCacheImpl::GetClientsAsValue() const {
  SanityCheckClients();
  std::vector<base::Value> client_list;
  for (const auto& domain_and_client : clients_) {
    const Client& client = domain_and_client.second;
    client_list.push_back(GetClientAsValue(client));
  }
  return base::Value(std::move(client_list));
}

size_t ReportingCacheImpl::GetEndpointCount() const {
  return endpoints_.size();
}

void ReportingCacheImpl::Flush() {
  if (context_->IsClientDataPersisted())
    store()->Flush();
}

ReportingEndpoint ReportingCacheImpl::GetEndpointForTesting(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) const {
  SanityCheckClients();
  for (const auto& group_key_and_endpoint : endpoints_) {
    const ReportingEndpoint& endpoint = group_key_and_endpoint.second;
    if (endpoint.group_key == group_key && endpoint.info.url == url)
      return endpoint;
  }
  return ReportingEndpoint();
}

bool ReportingCacheImpl::EndpointGroupExistsForTesting(
    const ReportingEndpointGroupKey& group_key,
    OriginSubdomains include_subdomains,
    base::Time expires) const {
  SanityCheckClients();
  for (const auto& key_and_group : endpoint_groups_) {
    const CachedReportingEndpointGroup& endpoint_group = key_and_group.second;
    if (endpoint_group.group_key == group_key &&
        endpoint_group.include_subdomains == include_subdomains) {
      if (expires != base::Time())
        return endpoint_group.expires == expires;
      return true;
    }
  }
  return false;
}

bool ReportingCacheImpl::ClientExistsForTesting(
    const NetworkIsolationKey& network_isolation_key,
    const url::Origin& origin) const {
  SanityCheckClients();
  for (const auto& domain_and_client : clients_) {
    const Client& client = domain_and_client.second;
    DCHECK_EQ(client.origin.host(), domain_and_client.first);
    if (client.network_isolation_key == network_isolation_key &&
        client.origin == origin) {
      return true;
    }
  }
  return false;
}

size_t ReportingCacheImpl::GetEndpointGroupCountForTesting() const {
  return endpoint_groups_.size();
}

size_t ReportingCacheImpl::GetClientCountForTesting() const {
  return clients_.size();
}

void ReportingCacheImpl::SetEndpointForTesting(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url,
    OriginSubdomains include_subdomains,
    base::Time expires,
    int priority,
    int weight) {
  ClientMap::iterator client_it = FindClientIt(group_key);
  // If the client doesn't yet exist, add it.
  if (client_it == clients_.end()) {
    Client new_client(group_key.network_isolation_key, group_key.origin);
    std::string domain = group_key.origin.host();
    client_it = clients_.insert(std::make_pair(domain, std::move(new_client)));
  }

  base::Time now = clock().Now();

  EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
  // If the endpoint group doesn't yet exist, add it.
  if (group_it == endpoint_groups_.end()) {
    CachedReportingEndpointGroup new_group(group_key, include_subdomains,
                                           expires, now);
    group_it =
        endpoint_groups_.insert(std::make_pair(group_key, std::move(new_group)))
            .first;
    client_it->second.endpoint_group_names.insert(group_key.group_name);
  } else {
    // Otherwise, update the existing entry
    group_it->second.include_subdomains = include_subdomains;
    group_it->second.expires = expires;
    group_it->second.last_used = now;
  }

  MarkEndpointGroupAndClientUsed(client_it, group_it, now);

  EndpointMap::iterator endpoint_it = FindEndpointIt(group_key, url);
  // If the endpoint doesn't yet exist, add it.
  if (endpoint_it == endpoints_.end()) {
    ReportingEndpoint::EndpointInfo info;
    info.url = std::move(url);
    info.priority = priority;
    info.weight = weight;
    ReportingEndpoint new_endpoint(group_key, info);
    endpoint_it =
        endpoints_.insert(std::make_pair(group_key, std::move(new_endpoint)));
    AddEndpointItToIndex(endpoint_it);
    ++client_it->second.endpoint_count;
  } else {
    // Otherwise, update the existing entry
    endpoint_it->second.info.priority = priority;
    endpoint_it->second.info.weight = weight;
  }

  EnforcePerClientAndGlobalEndpointLimits(client_it);
  SanityCheckClients();
  context_->NotifyCachedClientsUpdated();
}

ReportingCacheImpl::Client::Client(
    const NetworkIsolationKey& network_isolation_key,
    const url::Origin& origin)
    : network_isolation_key(network_isolation_key), origin(origin) {}

ReportingCacheImpl::Client::Client(const Client& other) = default;

ReportingCacheImpl::Client::Client(Client&& other) = default;

ReportingCacheImpl::Client& ReportingCacheImpl::Client::operator=(
    const Client& other) = default;

ReportingCacheImpl::Client& ReportingCacheImpl::Client::operator=(
    Client&& other) = default;

ReportingCacheImpl::Client::~Client() = default;

ReportingCacheImpl::ReportSet::const_iterator
ReportingCacheImpl::FindReportToEvict() const {
  ReportSet::const_iterator to_evict = reports_.end();

  for (auto it = reports_.begin(); it != reports_.end(); ++it) {
    // Don't evict pending or doomed reports.
    if (it->get()->IsUploadPending())
      continue;
    if (to_evict == reports_.end() ||
        it->get()->queued < to_evict->get()->queued) {
      to_evict = it;
    }
  }

  return to_evict;
}

void ReportingCacheImpl::SanityCheckClients() const {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t total_endpoint_count = 0;
  size_t total_endpoint_group_count = 0;
  std::set<std::pair<NetworkIsolationKey, url::Origin>>
      nik_origin_pairs_in_cache;

  for (const auto& domain_and_client : clients_) {
    const std::string& domain = domain_and_client.first;
    const Client& client = domain_and_client.second;
    total_endpoint_count += client.endpoint_count;
    total_endpoint_group_count += SanityCheckClient(domain, client);

    // We have not seen a duplicate client with the same NIK and origin.
    DCHECK(!base::Contains(
        nik_origin_pairs_in_cache,
        std::make_pair(client.network_isolation_key, client.origin)));
    nik_origin_pairs_in_cache.insert(
        std::make_pair(client.network_isolation_key, client.origin));
  }

  // Global endpoint cap is respected.
  DCHECK_LE(GetEndpointCount(), context_->policy().max_endpoint_count);

  // All the endpoints and endpoint groups are accounted for.
  DCHECK_EQ(total_endpoint_count, endpoints_.size());
  DCHECK_EQ(total_endpoint_group_count, endpoint_groups_.size());

  // All the endpoints are indexed properly.
  DCHECK_EQ(total_endpoint_count, endpoint_its_by_url_.size());
  for (const auto& url_and_endpoint_it : endpoint_its_by_url_) {
    DCHECK_EQ(url_and_endpoint_it.first,
              url_and_endpoint_it.second->second.info.url);
  }
}

size_t ReportingCacheImpl::SanityCheckClient(const std::string& domain,
                                             const Client& client) const {
  // Each client is keyed by its domain name.
  DCHECK_EQ(domain, client.origin.host());
  // Client is not empty (has at least one group)
  DCHECK(!client.endpoint_group_names.empty());

  size_t endpoint_count_in_client = 0;
  size_t endpoint_group_count_in_client = 0;

  for (const std::string& group_name : client.endpoint_group_names) {
    size_t groups_with_name = 0;
    for (const auto& key_and_group : endpoint_groups_) {
      const ReportingEndpointGroupKey& key = key_and_group.first;
      if (key.origin == client.origin &&
          key.network_isolation_key == client.network_isolation_key &&
          key.group_name == group_name) {
        ++endpoint_group_count_in_client;
        ++groups_with_name;
        endpoint_count_in_client +=
            SanityCheckEndpointGroup(key, key_and_group.second);
      }
    }
    DCHECK_EQ(1u, groups_with_name);
  }
  // Client has the correct endpoint count.
  DCHECK_EQ(client.endpoint_count, endpoint_count_in_client);
  // Per-client endpoint cap is respected.
  DCHECK_LE(client.endpoint_count, context_->policy().max_endpoints_per_origin);

  // Note: Not checking last_used time here because base::Time is not
  // guaranteed to be monotonically non-decreasing.

  return endpoint_group_count_in_client;
}

size_t ReportingCacheImpl::SanityCheckEndpointGroup(
    const ReportingEndpointGroupKey& key,
    const CachedReportingEndpointGroup& group) const {
  size_t endpoint_count_in_group = 0;

  // Each group is keyed by its origin and name.
  DCHECK(key == group.group_key);

  // Group is not empty (has at least one endpoint)
  DCHECK_LE(0u, GetEndpointCountInGroup(group.group_key));

  // Note: Not checking expiry here because expired groups are allowed to
  // linger in the cache until they are garbage collected.

  std::set<GURL> endpoint_urls_in_group;

  const auto group_range = endpoints_.equal_range(key);
  for (auto it = group_range.first; it != group_range.second; ++it) {
    const ReportingEndpoint& endpoint = it->second;

    SanityCheckEndpoint(key, endpoint, it);

    // We have not seen a duplicate endpoint with the same URL in this
    // group.
    DCHECK(!base::Contains(endpoint_urls_in_group, endpoint.info.url));
    endpoint_urls_in_group.insert(endpoint.info.url);

    ++endpoint_count_in_group;
  }

  return endpoint_count_in_group;
}

void ReportingCacheImpl::SanityCheckEndpoint(
    const ReportingEndpointGroupKey& key,
    const ReportingEndpoint& endpoint,
    EndpointMap::const_iterator endpoint_it) const {
  // Origin and group name match.
  DCHECK(key == endpoint.group_key);

  // Priority and weight are nonnegative integers.
  DCHECK_LE(0, endpoint.info.priority);
  DCHECK_LE(0, endpoint.info.weight);

  // The endpoint is in the |endpoint_its_by_url_| index.
  DCHECK(base::Contains(endpoint_its_by_url_, endpoint.info.url));
  auto url_range = endpoint_its_by_url_.equal_range(endpoint.info.url);
  std::vector<EndpointMap::iterator> endpoint_its_for_url;
  for (auto index_it = url_range.first; index_it != url_range.second;
       ++index_it) {
    endpoint_its_for_url.push_back(index_it->second);
  }
  DCHECK(base::Contains(endpoint_its_for_url, endpoint_it));
#endif  // DCHECK_IS_ON()
}

ReportingCacheImpl::ClientMap::iterator ReportingCacheImpl::FindClientIt(
    const NetworkIsolationKey& network_isolation_key,
    const url::Origin& origin) {
  // TODO(chlily): Limit the number of clients per domain to prevent an attacker
  // from installing many Reporting policies for different port numbers on the
  // same host.
  const auto domain_range = clients_.equal_range(origin.host());
  for (auto it = domain_range.first; it != domain_range.second; ++it) {
    if (it->second.network_isolation_key == network_isolation_key &&
        it->second.origin == origin) {
      return it;
    }
  }
  return clients_.end();
}

ReportingCacheImpl::ClientMap::iterator ReportingCacheImpl::FindClientIt(
    const ReportingEndpointGroupKey& group_key) {
  return FindClientIt(group_key.network_isolation_key, group_key.origin);
}

ReportingCacheImpl::EndpointGroupMap::iterator
ReportingCacheImpl::FindEndpointGroupIt(
    const ReportingEndpointGroupKey& group_key) {
  return endpoint_groups_.find(group_key);
}

ReportingCacheImpl::EndpointMap::iterator ReportingCacheImpl::FindEndpointIt(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  const auto group_range = endpoints_.equal_range(group_key);
  for (auto it = group_range.first; it != group_range.second; ++it) {
    if (it->second.info.url == url)
      return it;
  }
  return endpoints_.end();
}

ReportingCacheImpl::ClientMap::iterator ReportingCacheImpl::AddOrUpdateClient(
    Client new_client) {
  ClientMap::iterator client_it =
      FindClientIt(new_client.network_isolation_key, new_client.origin);

  // Add a new client for this NIK and origin.
  if (client_it == clients_.end()) {
    std::string domain = new_client.origin.host();
    client_it = clients_.insert(
        std::make_pair(std::move(domain), std::move(new_client)));
  } else {
    // If an entry already existed, just update it.
    Client& old_client = client_it->second;
    old_client.endpoint_count = new_client.endpoint_count;
    old_client.endpoint_group_names =
        std::move(new_client.endpoint_group_names);
    old_client.last_used = new_client.last_used;
  }

  // Note: SanityCheckClients() may fail here because we may be over the
  // global/per-origin endpoint limits.
  return client_it;
}

void ReportingCacheImpl::AddOrUpdateEndpointGroup(
    CachedReportingEndpointGroup new_group) {
  EndpointGroupMap::iterator group_it =
      FindEndpointGroupIt(new_group.group_key);

  // Add a new endpoint group for this origin and group name.
  if (group_it == endpoint_groups_.end()) {
    if (context_->IsClientDataPersisted())
      store()->AddReportingEndpointGroup(new_group);

    endpoint_groups_.insert(
        std::make_pair(new_group.group_key, std::move(new_group)));
    return;
  }

  // If an entry already existed, just update it.
  CachedReportingEndpointGroup& old_group = group_it->second;
  old_group.include_subdomains = new_group.include_subdomains;
  old_group.expires = new_group.expires;
  old_group.last_used = new_group.last_used;

  if (context_->IsClientDataPersisted())
    store()->UpdateReportingEndpointGroupDetails(new_group);

  // Note: SanityCheckClients() may fail here because we have not yet
  // added/updated the Client yet.
}

void ReportingCacheImpl::AddOrUpdateEndpoint(ReportingEndpoint new_endpoint) {
  EndpointMap::iterator endpoint_it =
      FindEndpointIt(new_endpoint.group_key, new_endpoint.info.url);

  // Add a new endpoint for this origin, group, and url.
  if (endpoint_it == endpoints_.end()) {
    if (context_->IsClientDataPersisted())
      store()->AddReportingEndpoint(new_endpoint);

    EndpointMap::iterator endpoint_it = endpoints_.insert(
        std::make_pair(new_endpoint.group_key, std::move(new_endpoint)));
    AddEndpointItToIndex(endpoint_it);

    // If the client already exists, update its endpoint count.
    ClientMap::iterator client_it = FindClientIt(endpoint_it->second.group_key);
    if (client_it != clients_.end())
      ++client_it->second.endpoint_count;
    return;
  }

  // If an entry already existed, just update it.
  ReportingEndpoint& old_endpoint = endpoint_it->second;
  old_endpoint.info.priority = new_endpoint.info.priority;
  old_endpoint.info.weight = new_endpoint.info.weight;
  // |old_endpoint.stats| stays the same.

  if (context_->IsClientDataPersisted())
    store()->UpdateReportingEndpointDetails(new_endpoint);

  // Note: SanityCheckClients() may fail here because we have not yet
  // added/updated the Client yet.
}

void ReportingCacheImpl::RemoveEndpointsInGroupOtherThan(
    const ReportingEndpointGroupKey& group_key,
    const std::set<GURL>& endpoints_to_keep_urls) {
  EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
  if (group_it == endpoint_groups_.end())
    return;
  ClientMap::iterator client_it = FindClientIt(group_key);
  // Normally a group would not exist without a client for that origin, but
  // this can actually happen during header parsing if a header for an origin
  // without a pre-existing configuration erroneously contains multiple groups
  // with the same name. In that case, we assume here that they meant to set all
  // of those same-name groups as one group, so we don't remove anything.
  if (client_it == clients_.end())
    return;

  const auto group_range = endpoints_.equal_range(group_key);
  for (auto it = group_range.first; it != group_range.second;) {
    if (base::Contains(endpoints_to_keep_urls, it->second.info.url)) {
      ++it;
      continue;
    }

    // This may invalidate |group_it| (and also possibly |client_it|), but only
    // if we are processing the last remaining endpoint in the group.
    base::Optional<EndpointMap::iterator> next_it =
        RemoveEndpointInternal(client_it, group_it, it);
    if (!next_it.has_value())
      return;
    it = next_it.value();
  }
}

void ReportingCacheImpl::RemoveEndpointGroupsForClientOtherThan(
    const NetworkIsolationKey& network_isolation_key,
    const url::Origin& origin,
    const std::set<std::string>& groups_to_keep_names) {
  ClientMap::iterator client_it = FindClientIt(network_isolation_key, origin);
  if (client_it == clients_.end())
    return;

  std::set<std::string>& old_group_names =
      client_it->second.endpoint_group_names;
  std::vector<std::string> groups_to_remove_names =
      base::STLSetDifference<std::vector<std::string>>(old_group_names,
                                                       groups_to_keep_names);

  for (const std::string& group_name : groups_to_remove_names) {
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(
        ReportingEndpointGroupKey(network_isolation_key, origin, group_name));
    RemoveEndpointGroupInternal(client_it, group_it);
  }
}

std::vector<ReportingEndpoint> ReportingCacheImpl::GetEndpointsInGroup(
    const ReportingEndpointGroupKey& group_key) const {
  const auto group_range = endpoints_.equal_range(group_key);
  std::vector<ReportingEndpoint> endpoints_out;
  for (auto it = group_range.first; it != group_range.second; ++it) {
    endpoints_out.push_back(it->second);
  }
  return endpoints_out;
}

size_t ReportingCacheImpl::GetEndpointCountInGroup(
    const ReportingEndpointGroupKey& group_key) const {
  return endpoints_.count(group_key);
}

void ReportingCacheImpl::MarkEndpointGroupAndClientUsed(
    ClientMap::iterator client_it,
    EndpointGroupMap::iterator group_it,
    base::Time now) {
  group_it->second.last_used = now;
  client_it->second.last_used = now;
  if (context_->IsClientDataPersisted())
    store()->UpdateReportingEndpointGroupAccessTime(group_it->second);
}

base::Optional<ReportingCacheImpl::EndpointMap::iterator>
ReportingCacheImpl::RemoveEndpointInternal(ClientMap::iterator client_it,
                                           EndpointGroupMap::iterator group_it,
                                           EndpointMap::iterator endpoint_it) {
  DCHECK(client_it != clients_.end());
  DCHECK(group_it != endpoint_groups_.end());
  DCHECK(endpoint_it != endpoints_.end());

  const ReportingEndpointGroupKey& group_key = endpoint_it->first;
  // If this is the only endpoint in the group, then removing it will cause the
  // group to become empty, so just remove the whole group. The client may also
  // be removed if it becomes empty.
  if (endpoints_.count(group_key) == 1) {
    RemoveEndpointGroupInternal(client_it, group_it);
    return base::nullopt;
  }
  // Otherwise, there are other endpoints in the group, so there is no chance
  // of needing to remove the group/client. Just remove this endpoint and
  // update the client's endpoint count.
  DCHECK_GT(client_it->second.endpoint_count, 1u);
  RemoveEndpointItFromIndex(endpoint_it);
  --client_it->second.endpoint_count;
  if (context_->IsClientDataPersisted())
    store()->DeleteReportingEndpoint(endpoint_it->second);
  return endpoints_.erase(endpoint_it);
}

base::Optional<ReportingCacheImpl::EndpointGroupMap::iterator>
ReportingCacheImpl::RemoveEndpointGroupInternal(
    ClientMap::iterator client_it,
    EndpointGroupMap::iterator group_it,
    size_t* num_endpoints_removed) {
  DCHECK(client_it != clients_.end());
  DCHECK(group_it != endpoint_groups_.end());
  const ReportingEndpointGroupKey& group_key = group_it->first;

  // Remove the endpoints for this group.
  const auto group_range = endpoints_.equal_range(group_key);
  size_t endpoints_removed =
      std::distance(group_range.first, group_range.second);
  DCHECK_GT(endpoints_removed, 0u);
  if (num_endpoints_removed)
    *num_endpoints_removed += endpoints_removed;
  for (auto it = group_range.first; it != group_range.second; ++it) {
    if (context_->IsClientDataPersisted())
      store()->DeleteReportingEndpoint(it->second);

    RemoveEndpointItFromIndex(it);
  }
  endpoints_.erase(group_range.first, group_range.second);

  // Update the client's endpoint count.
  Client& client = client_it->second;
  client.endpoint_count -= endpoints_removed;

  // Remove endpoint group from client.
  size_t erased_from_client =
      client.endpoint_group_names.erase(group_key.group_name);
  DCHECK_EQ(1u, erased_from_client);

  if (context_->IsClientDataPersisted())
    store()->DeleteReportingEndpointGroup(group_it->second);

  base::Optional<EndpointGroupMap::iterator> rv =
      endpoint_groups_.erase(group_it);

  // Delete client if empty.
  if (client.endpoint_count == 0) {
    DCHECK(client.endpoint_group_names.empty());
    clients_.erase(client_it);
    rv = base::nullopt;
  }
  return rv;
}

ReportingCacheImpl::ClientMap::iterator
ReportingCacheImpl::RemoveClientInternal(ClientMap::iterator client_it) {
  DCHECK(client_it != clients_.end());
  const Client& client = client_it->second;

  // Erase all groups in this client, and all endpoints in those groups.
  for (const std::string& group_name : client.endpoint_group_names) {
    ReportingEndpointGroupKey group_key(client.network_isolation_key,
                                        client.origin, group_name);
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
    if (context_->IsClientDataPersisted())
      store()->DeleteReportingEndpointGroup(group_it->second);
    endpoint_groups_.erase(group_it);

    const auto group_range = endpoints_.equal_range(group_key);
    for (auto it = group_range.first; it != group_range.second; ++it) {
      if (context_->IsClientDataPersisted())
        store()->DeleteReportingEndpoint(it->second);

      RemoveEndpointItFromIndex(it);
    }
    endpoints_.erase(group_range.first, group_range.second);
  }

  return clients_.erase(client_it);
}

void ReportingCacheImpl::EnforcePerClientAndGlobalEndpointLimits(
    ClientMap::iterator client_it) {
  DCHECK(client_it != clients_.end());
  size_t client_endpoint_count = client_it->second.endpoint_count;
  // TODO(chlily): This is actually a limit on the endpoints for a given client
  // (for a NIK, origin pair). Rename this.
  size_t max_endpoints_per_origin = context_->policy().max_endpoints_per_origin;
  if (client_endpoint_count > max_endpoints_per_origin) {
    EvictEndpointsFromClient(client_it,
                             client_endpoint_count - max_endpoints_per_origin);
  }

  size_t max_endpoint_count = context_->policy().max_endpoint_count;
  while (GetEndpointCount() > max_endpoint_count) {
    // Find the stalest client (arbitrarily pick the first one if there are
    // multiple).
    ClientMap::iterator to_evict = clients_.end();
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
      const Client& client = it->second;
      if (to_evict == clients_.end() ||
          client.last_used < to_evict->second.last_used) {
        to_evict = it;
      }
    }

    DCHECK(to_evict != clients_.end());

    // Evict endpoints from the chosen client.
    size_t num_to_evict = GetEndpointCount() - max_endpoint_count;
    EvictEndpointsFromClient(
        to_evict, std::min(to_evict->second.endpoint_count, num_to_evict));
  }
}

void ReportingCacheImpl::EvictEndpointsFromClient(ClientMap::iterator client_it,
                                                  size_t endpoints_to_evict) {
  DCHECK_GT(endpoints_to_evict, 0u);
  DCHECK(client_it != clients_.end());
  const Client& client = client_it->second;
  // Cache this value as |client| may be deleted.
  size_t client_endpoint_count = client.endpoint_count;
  const NetworkIsolationKey& network_isolation_key =
      client.network_isolation_key;
  const url::Origin& origin = client.origin;

  DCHECK_GE(client_endpoint_count, endpoints_to_evict);
  if (endpoints_to_evict == client_endpoint_count) {
    RemoveClientInternal(client_it);
    return;
  }

  size_t endpoints_removed = 0;
  bool client_deleted =
      RemoveExpiredOrStaleGroups(client_it, &endpoints_removed);
  // If we deleted the whole client, there is nothing left to do.
  if (client_deleted) {
    DCHECK_EQ(endpoints_removed, client_endpoint_count);
    return;
  }

  DCHECK(!client.endpoint_group_names.empty());

  while (endpoints_removed < endpoints_to_evict) {
    DCHECK_GT(client_it->second.endpoint_count, 0u);
    // Find the stalest group with the most endpoints.
    EndpointGroupMap::iterator stalest_group_it = endpoint_groups_.end();
    size_t stalest_group_endpoint_count = 0;
    for (const std::string& group_name : client.endpoint_group_names) {
      ReportingEndpointGroupKey group_key(network_isolation_key, origin,
                                          group_name);
      EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
      size_t group_endpoint_count = GetEndpointCountInGroup(group_key);

      const CachedReportingEndpointGroup& group = group_it->second;
      if (stalest_group_it == endpoint_groups_.end() ||
          group.last_used < stalest_group_it->second.last_used ||
          (group.last_used == stalest_group_it->second.last_used &&
           group_endpoint_count > stalest_group_endpoint_count)) {
        stalest_group_it = group_it;
        stalest_group_endpoint_count = group_endpoint_count;
      }
    }
    DCHECK(stalest_group_it != endpoint_groups_.end());

    // Evict the least important (lowest priority, lowest weight) endpoint.
    EvictEndpointFromGroup(client_it, stalest_group_it);
    ++endpoints_removed;
  }
}

void ReportingCacheImpl::EvictEndpointFromGroup(
    ClientMap::iterator client_it,
    EndpointGroupMap::iterator group_it) {
  const ReportingEndpointGroupKey& group_key = group_it->first;
  const auto group_range = endpoints_.equal_range(group_key);
  EndpointMap::iterator endpoint_to_evict_it = endpoints_.end();
  for (auto it = group_range.first; it != group_range.second; ++it) {
    const ReportingEndpoint& endpoint = it->second;
    if (endpoint_to_evict_it == endpoints_.end() ||
        // Lower priority = higher numerical value of |priority|.
        endpoint.info.priority > endpoint_to_evict_it->second.info.priority ||
        (endpoint.info.priority == endpoint_to_evict_it->second.info.priority &&
         endpoint.info.weight < endpoint_to_evict_it->second.info.weight)) {
      endpoint_to_evict_it = it;
    }
  }
  DCHECK(endpoint_to_evict_it != endpoints_.end());

  RemoveEndpointInternal(client_it, group_it, endpoint_to_evict_it);
}

bool ReportingCacheImpl::RemoveExpiredOrStaleGroups(
    ClientMap::iterator client_it,
    size_t* num_endpoints_removed) {
  base::Time now = clock().Now();
  // Make a copy of this because |client_it| may be invalidated.
  std::set<std::string> groups_in_client_names(
      client_it->second.endpoint_group_names);

  for (const std::string& group_name : groups_in_client_names) {
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(
        ReportingEndpointGroupKey(client_it->second.network_isolation_key,
                                  client_it->second.origin, group_name));
    DCHECK(group_it != endpoint_groups_.end());
    const CachedReportingEndpointGroup& group = group_it->second;
    if (group.expires < now ||
        now - group.last_used > context_->policy().max_group_staleness) {
      // May delete the client, invalidating |client_it|, but only if we are
      // processing the last remaining group.
      if (!RemoveEndpointGroupInternal(client_it, group_it,
                                       num_endpoints_removed)
               .has_value()) {
        return true;
      }
    }
  }

  return false;
}

void ReportingCacheImpl::AddEndpointItToIndex(
    EndpointMap::iterator endpoint_it) {
  const GURL& url = endpoint_it->second.info.url;
  endpoint_its_by_url_.insert(std::make_pair(url, endpoint_it));
}

void ReportingCacheImpl::RemoveEndpointItFromIndex(
    EndpointMap::iterator endpoint_it) {
  const GURL& url = endpoint_it->second.info.url;
  auto url_range = endpoint_its_by_url_.equal_range(url);
  for (auto it = url_range.first; it != url_range.second; ++it) {
    if (it->second == endpoint_it) {
      endpoint_its_by_url_.erase(it);
      return;
    }
  }
}

base::Value ReportingCacheImpl::GetClientAsValue(const Client& client) const {
  base::Value client_dict(base::Value::Type::DICTIONARY);
  client_dict.SetKey("network_isolation_key",
                     base::Value(client.network_isolation_key.ToDebugString()));
  client_dict.SetKey("origin", base::Value(client.origin.Serialize()));

  std::vector<base::Value> group_list;
  for (const std::string& group_name : client.endpoint_group_names) {
    ReportingEndpointGroupKey group_key(client.network_isolation_key,
                                        client.origin, group_name);
    const CachedReportingEndpointGroup& group = endpoint_groups_.at(group_key);
    group_list.push_back(GetEndpointGroupAsValue(group));
  }

  client_dict.SetKey("groups", base::Value(std::move(group_list)));

  return client_dict;
}

base::Value ReportingCacheImpl::GetEndpointGroupAsValue(
    const CachedReportingEndpointGroup& group) const {
  base::Value group_dict(base::Value::Type::DICTIONARY);
  group_dict.SetKey("name", base::Value(group.group_key.group_name));
  group_dict.SetKey("expires",
                    base::Value(NetLog::TimeToString(group.expires)));
  group_dict.SetKey(
      "includeSubdomains",
      base::Value(group.include_subdomains == OriginSubdomains::INCLUDE));

  std::vector<base::Value> endpoint_list;

  const auto group_range = endpoints_.equal_range(group.group_key);
  for (auto it = group_range.first; it != group_range.second; ++it) {
    const ReportingEndpoint& endpoint = it->second;
    endpoint_list.push_back(GetEndpointAsValue(endpoint));
  }

  group_dict.SetKey("endpoints", base::Value(std::move(endpoint_list)));

  return group_dict;
}

base::Value ReportingCacheImpl::GetEndpointAsValue(
    const ReportingEndpoint& endpoint) const {
  base::Value endpoint_dict(base::Value::Type::DICTIONARY);
  endpoint_dict.SetKey("url", base::Value(endpoint.info.url.spec()));
  endpoint_dict.SetKey("priority", base::Value(endpoint.info.priority));
  endpoint_dict.SetKey("weight", base::Value(endpoint.info.weight));

  const ReportingEndpoint::Statistics& stats = endpoint.stats;
  base::Value successful_dict(base::Value::Type::DICTIONARY);
  successful_dict.SetKey("uploads", base::Value(stats.successful_uploads));
  successful_dict.SetKey("reports", base::Value(stats.successful_reports));
  endpoint_dict.SetKey("successful", std::move(successful_dict));
  base::Value failed_dict(base::Value::Type::DICTIONARY);
  failed_dict.SetKey("uploads", base::Value(stats.attempted_uploads -
                                            stats.successful_uploads));
  failed_dict.SetKey("reports", base::Value(stats.attempted_reports -
                                            stats.successful_reports));
  endpoint_dict.SetKey("failed", std::move(failed_dict));

  return endpoint_dict;
}

}  // namespace net
