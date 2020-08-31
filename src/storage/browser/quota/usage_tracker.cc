// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/usage_tracker.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "storage/browser/quota/client_usage_tracker.h"

namespace storage {

namespace {

void DidGetGlobalUsageForLimitedGlobalUsage(UsageCallback callback,
                                            int64_t total_global_usage,
                                            int64_t global_unlimited_usage) {
  std::move(callback).Run(total_global_usage - global_unlimited_usage);
}

void StripUsageWithBreakdownCallback(
    UsageCallback callback,
    int64_t usage,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  std::move(callback).Run(usage);
}

}  // namespace

struct UsageTracker::AccumulateInfo {
  AccumulateInfo() = default;
  ~AccumulateInfo() = default;

  size_t pending_clients = 0;
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr usage_breakdown =
      blink::mojom::UsageBreakdown::New();
};

UsageTracker::UsageTracker(
    const std::vector<scoped_refptr<QuotaClient>>& clients,
    blink::mojom::StorageType type,
    SpecialStoragePolicy* special_storage_policy)
    : type_(type) {
  for (const auto& client : clients) {
    if (client->DoesSupport(type)) {
      client_tracker_map_[client->type()] =
          std::make_unique<ClientUsageTracker>(this, client, type,
                                               special_storage_policy);
    }
  }
}

UsageTracker::~UsageTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UsageTracker::GetGlobalLimitedUsage(UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_usage_callbacks_.empty()) {
    global_usage_callbacks_.emplace_back(base::BindOnce(
        &DidGetGlobalUsageForLimitedGlobalUsage, std::move(callback)));
    return;
  }

  global_limited_usage_callbacks_.emplace_back(std::move(callback));
  if (global_limited_usage_callbacks_.size() > 1)
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // Calling GetGlobalLimitedUsage(accumulator) may synchronously
  // return if the usage is cached, which may in turn dispatch
  // the completion callback before we finish looping over
  // all clients (because info->pending_clients may reach 0
  // during the loop).
  // To avoid this, we add one more pending client as a sentinel
  // and fire the sentinel callback at the end.
  info->pending_clients = client_tracker_map_.size() + 1;
  auto accumulator =
      base::BindRepeating(&UsageTracker::AccumulateClientGlobalLimitedUsage,
                          weak_factory_.GetWeakPtr(), base::Owned(info));

  for (const auto& client_type_and_tracker : client_tracker_map_)
    client_type_and_tracker.second->GetGlobalLimitedUsage(accumulator);

  // Fire the sentinel as we've now called GetGlobalUsage for all clients.
  accumulator.Run(0);
}

void UsageTracker::GetGlobalUsage(GlobalUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_usage_callbacks_.emplace_back(std::move(callback));
  if (global_usage_callbacks_.size() > 1)
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // Calling GetGlobalUsage(accumulator) may synchronously
  // return if the usage is cached, which may in turn dispatch
  // the completion callback before we finish looping over
  // all clients (because info->pending_clients may reach 0
  // during the loop).
  // To avoid this, we add one more pending client as a sentinel
  // and fire the sentinel callback at the end.
  info->pending_clients = client_tracker_map_.size() + 1;
  auto accumulator =
      base::BindRepeating(&UsageTracker::AccumulateClientGlobalUsage,
                          weak_factory_.GetWeakPtr(), base::Owned(info));

  for (const auto& client_type_and_tracker : client_tracker_map_)
    client_type_and_tracker.second->GetGlobalUsage(accumulator);

  // Fire the sentinel as we've now called GetGlobalUsage for all clients.
  accumulator.Run(0, 0);
}

void UsageTracker::GetHostUsage(const std::string& host,
                                UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UsageTracker::GetHostUsageWithBreakdown(
      host,
      base::BindOnce(&StripUsageWithBreakdownCallback, std::move(callback)));
}

void UsageTracker::GetHostUsageWithBreakdown(
    const std::string& host,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<UsageWithBreakdownCallback>& host_callbacks =
      host_usage_callbacks_[host];
  host_callbacks.emplace_back(std::move(callback));
  if (host_callbacks.size() > 1)
    return;

  AccumulateInfo* info = new AccumulateInfo;
  // We use BarrierClosure here instead of manually counting pending_clients.
  base::RepeatingClosure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendHostUsageWithBreakdown,
                     weak_factory_.GetWeakPtr(), base::Owned(info), host));

  for (const auto& client_type_and_tracker : client_tracker_map_) {
    client_type_and_tracker.second->GetHostUsage(
        host, base::BindOnce(&UsageTracker::AccumulateClientHostUsage,
                             weak_factory_.GetWeakPtr(), barrier, info, host,
                             client_type_and_tracker.first));
  }
}

void UsageTracker::UpdateUsageCache(QuotaClientType client_type,
                                    const url::Origin& origin,
                                    int64_t delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_tracker_map_.count(client_type));
  ClientUsageTracker* client_tracker = client_tracker_map_[client_type].get();
  client_tracker->UpdateUsageCache(origin, delta);
}

int64_t UsageTracker::GetCachedUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t usage = 0;
  for (const auto& client_type_and_tracker : client_tracker_map_)
    usage += client_type_and_tracker.second->GetCachedUsage();
  return usage;
}

std::map<std::string, int64_t> UsageTracker::GetCachedHostsUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, int64_t> host_usage;
  for (const auto& client_type_and_tracker : client_tracker_map_) {
    std::map<std::string, int64_t> client_host_usage =
        client_type_and_tracker.second->GetCachedHostsUsage();
    for (const auto& host_and_usage : client_host_usage)
      host_usage[host_and_usage.first] += host_and_usage.second;
  }
  return host_usage;
}

std::map<url::Origin, int64_t> UsageTracker::GetCachedOriginsUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<url::Origin, int64_t> origin_usage;
  for (const auto& client_type_and_tracker : client_tracker_map_) {
    std::map<url::Origin, int64_t> client_origin_usage =
        client_type_and_tracker.second->GetCachedOriginsUsage();
    for (const auto& origin_and_usage : client_origin_usage)
      origin_usage[origin_and_usage.first] += origin_and_usage.second;
  }
  return origin_usage;
}

std::set<url::Origin> UsageTracker::GetCachedOrigins() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<url::Origin> origins;
  for (const auto& client_type_and_tracker : client_tracker_map_) {
    std::set<url::Origin> client_origins =
        client_type_and_tracker.second->GetCachedOrigins();
    for (const auto& client_origin : client_origins)
      origins.insert(client_origin);
  }
  return origins;
}

void UsageTracker::SetUsageCacheEnabled(QuotaClientType client_type,
                                        const url::Origin& origin,
                                        bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_tracker_map_.count(client_type));
  ClientUsageTracker* client_tracker = client_tracker_map_[client_type].get();

  client_tracker->SetUsageCacheEnabled(origin, enabled);
}

void UsageTracker::AccumulateClientGlobalLimitedUsage(AccumulateInfo* info,
                                                      int64_t limited_usage) {
  DCHECK_GT(info->pending_clients, 0U);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  info->usage += limited_usage;
  if (--info->pending_clients)
    return;

  // Moving callbacks out of the original vector handles the case where a
  // callback makes a new quota call.
  std::vector<UsageCallback> pending_callbacks;
  pending_callbacks.swap(global_limited_usage_callbacks_);
  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage);
}

void UsageTracker::AccumulateClientGlobalUsage(AccumulateInfo* info,
                                               int64_t usage,
                                               int64_t unlimited_usage) {
  DCHECK_GT(info->pending_clients, 0U);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  info->usage += usage;
  info->unlimited_usage += unlimited_usage;
  if (--info->pending_clients)
    return;

  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  // TODO(michaeln): The unlimited number is not trustworthy, it
  // can get out of whack when apps are installed or uninstalled.
  if (info->unlimited_usage > info->usage)
    info->unlimited_usage = info->usage;
  else if (info->unlimited_usage < 0)
    info->unlimited_usage = 0;

  // Moving callbacks out of the original vector early handles the case where a
  // callback makes a new quota call.
  std::vector<GlobalUsageCallback> pending_callbacks;
  pending_callbacks.swap(global_usage_callbacks_);
  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->unlimited_usage);
}

void UsageTracker::AccumulateClientHostUsage(base::OnceClosure callback,
                                             AccumulateInfo* info,
                                             const std::string& host,
                                             QuotaClientType client,
                                             int64_t usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  info->usage += usage;
  // Defend against confusing inputs from clients.
  if (info->usage < 0)
    info->usage = 0;

  switch (client) {
    case QuotaClientType::kFileSystem:
      info->usage_breakdown->fileSystem += usage;
      break;
    case QuotaClientType::kDatabase:
      info->usage_breakdown->webSql += usage;
      break;
    case QuotaClientType::kAppcache:
      info->usage_breakdown->appcache += usage;
      break;
    case QuotaClientType::kIndexedDatabase:
      info->usage_breakdown->indexedDatabase += usage;
      break;
    case QuotaClientType::kServiceWorkerCache:
      info->usage_breakdown->serviceWorkerCache += usage;
      break;
    case QuotaClientType::kServiceWorker:
      info->usage_breakdown->serviceWorker += usage;
      break;
    case QuotaClientType::kBackgroundFetch:
      info->usage_breakdown->backgroundFetch += usage;
      break;
  }

  std::move(callback).Run();
}

void UsageTracker::FinallySendHostUsageWithBreakdown(AccumulateInfo* info,
                                                     const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto host_it = host_usage_callbacks_.find(host);
  if (host_it == host_usage_callbacks_.end())
    return;

  std::vector<UsageWithBreakdownCallback> pending_callbacks;
  pending_callbacks.swap(host_it->second);
  DCHECK(pending_callbacks.size() > 0)
      << "host_usage_callbacks_ should only have non-empty callback lists";
  host_usage_callbacks_.erase(host_it);

  for (auto& callback : pending_callbacks) {
    std::move(callback).Run(info->usage, info->usage_breakdown->Clone());
  }
}

}  // namespace storage
