// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/usage_tracker.h"

#include <algorithm>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util.h"
#include "net/base/net_util.h"

namespace quota {

namespace {

bool SortByHost(const GURL& lhs, const GURL& rhs) {
  return net::GetHostOrSpecFromURL(lhs) > net::GetHostOrSpecFromURL(rhs);
}

}  // namespace

// A task class for getting the total amount of data used for a collection of
// origins.  This class is self-destructed.
class ClientUsageTracker::GatherUsageTaskBase : public QuotaTask {
 public:
  GatherUsageTaskBase(
      UsageTracker* tracker,
      QuotaClient* client)
      : QuotaTask(tracker),
        client_(client),
        tracker_(tracker),
        current_gathered_usage_(0),
        weak_factory_(this) {
    DCHECK(tracker_);
    DCHECK(client_);
    client_tracker_ = base::AsWeakPtr(
        tracker_->GetClientTracker(client_->id()));
    DCHECK(client_tracker_.get());
  }
  virtual ~GatherUsageTaskBase() {}

  // Get total usage for the given |origins|.
  void GetUsageForOrigins(const std::set<GURL>& origins, StorageType type) {
    DCHECK(original_task_runner()->BelongsToCurrentThread());
    if (!client_tracker()) {
      DeleteSoon();
      return;
    }
    // We do not get usage for origins for which we have valid usage cache.
    std::vector<GURL> origins_not_in_cache;
    current_gathered_usage_ = client_tracker()->GetCachedOriginsUsage(
        origins, &origins_not_in_cache);
    if (origins_not_in_cache.empty()) {
      CallCompleted();
      DeleteSoon();
      return;
    }

    // Sort them so we can detect when we've gathered all info for a particular
    // host in DidGetUsage.
    std::sort(origins_not_in_cache.begin(),
              origins_not_in_cache.end(), SortByHost);

    // First, fully populate the pending queue because GetOriginUsage may call
    // the completion callback immediately.
    for (std::vector<GURL>::const_iterator iter = origins_not_in_cache.begin();
         iter != origins_not_in_cache.end(); iter++)
      pending_origins_.push_back(*iter);

    for (std::vector<GURL>::const_iterator iter = origins_not_in_cache.begin();
         iter != origins_not_in_cache.end(); iter++)
      client_->GetOriginUsage(
          *iter,
          tracker_->type(),
          base::Bind(&GatherUsageTaskBase::DidGetUsage,
                     weak_factory_.GetWeakPtr()));
  }

 protected:
  virtual void DidGetOriginUsage(const GURL& origin, int64 usage) {}

  virtual void Aborted() OVERRIDE {
    DeleteSoon();
  }

  UsageTracker* tracker() const { return tracker_; }
  ClientUsageTracker* client_tracker() const { return client_tracker_.get(); }

  int64 current_gathered_usage() const { return current_gathered_usage_; }

 private:
  void DidGetUsage(int64 usage) {
    if (!client_tracker()) {
      DeleteSoon();
      return;
    }

    DCHECK(original_task_runner()->BelongsToCurrentThread());
    DCHECK(!pending_origins_.empty());

    // Defend against confusing inputs from QuotaClients.
    DCHECK_GE(usage, 0);
    if (usage < 0)
      usage = 0;
    current_gathered_usage_ += usage;

    // This code assumes DidGetUsage callbacks are called in the same
    // order as we dispatched GetOriginUsage calls.
    const GURL& origin = pending_origins_.front();
    std::string host = net::GetHostOrSpecFromURL(origin);
    client_tracker_->AddCachedOrigin(origin, usage);

    DidGetOriginUsage(origin, usage);

    pending_origins_.pop_front();
    if (pending_origins_.empty() ||
        host != net::GetHostOrSpecFromURL(pending_origins_.front())) {
      client_tracker_->AddCachedHost(host);
    }

    if (pending_origins_.empty()) {
      // We're done.
      CallCompleted();
      DeleteSoon();
    }
  }

  QuotaClient* client_;
  UsageTracker* tracker_;
  base::WeakPtr<ClientUsageTracker> client_tracker_;
  std::deque<GURL> pending_origins_;
  std::map<GURL, int64> origin_usage_map_;
  int64 current_gathered_usage_;
  base::WeakPtrFactory<GatherUsageTaskBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GatherUsageTaskBase);
};

// A task class for getting the total amount of data used for a given storage
// type.  This class is self-destructed.
class ClientUsageTracker::GatherGlobalUsageTask
    : public GatherUsageTaskBase {
 public:
  GatherGlobalUsageTask(
      UsageTracker* tracker,
      QuotaClient* client)
      : GatherUsageTaskBase(tracker, client),
        client_(client),
        non_cached_global_usage_(0),
        weak_factory_(this) {
    DCHECK(tracker);
    DCHECK(client);
  }
  virtual ~GatherGlobalUsageTask() {}

 protected:
  virtual void Run() OVERRIDE {
    client_->GetOriginsForType(tracker()->type(),
        base::Bind(&GatherUsageTaskBase::GetUsageForOrigins,
                   weak_factory_.GetWeakPtr()));
  }

  virtual void DidGetOriginUsage(const GURL& origin, int64 usage) OVERRIDE {
    if (!client_tracker()->IsUsageCacheEnabledForOrigin(origin)) {
      std::string host = net::GetHostOrSpecFromURL(origin);
      non_cached_usage_by_host_[host] += usage;
      non_cached_global_usage_ += usage;
    }
  }

  virtual void Completed() OVERRIDE {
    client_tracker()->GatherGlobalUsageComplete(current_gathered_usage(),
                                                non_cached_global_usage_,
                                                non_cached_usage_by_host_);
  }

 private:
  QuotaClient* client_;
  int64 non_cached_global_usage_;
  std::map<std::string, int64> non_cached_usage_by_host_;
  base::WeakPtrFactory<GatherUsageTaskBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GatherGlobalUsageTask);
};

// A task class for getting the total amount of data used for a given host.
// This class is self-destructed.
class ClientUsageTracker::GatherHostUsageTask
    : public GatherUsageTaskBase {
 public:
  GatherHostUsageTask(
      UsageTracker* tracker,
      QuotaClient* client,
      const std::string& host)
      : GatherUsageTaskBase(tracker, client),
        client_(client),
        host_(host),
        weak_factory_(this) {
    DCHECK(client_);
  }
  virtual ~GatherHostUsageTask() {}

 protected:
  virtual void Run() OVERRIDE {
    client_->GetOriginsForHost(tracker()->type(), host_,
        base::Bind(&GatherUsageTaskBase::GetUsageForOrigins,
                   weak_factory_.GetWeakPtr()));
  }

  virtual void Completed() OVERRIDE {
    client_tracker()->GatherHostUsageComplete(host_, current_gathered_usage());
  }

 private:
  QuotaClient* client_;
  std::string host_;
  base::WeakPtrFactory<GatherUsageTaskBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GatherHostUsageTask);
};

// UsageTracker ----------------------------------------------------------

UsageTracker::UsageTracker(const QuotaClientList& clients, StorageType type,
                           SpecialStoragePolicy* special_storage_policy)
    : type_(type),
      weak_factory_(this) {
  for (QuotaClientList::const_iterator iter = clients.begin();
      iter != clients.end();
      ++iter) {
    client_tracker_map_.insert(std::make_pair(
        (*iter)->id(),
        new ClientUsageTracker(this, *iter, type, special_storage_policy)));
  }
}

UsageTracker::~UsageTracker() {
  STLDeleteValues(&client_tracker_map_);
}

ClientUsageTracker* UsageTracker::GetClientTracker(QuotaClient::ID client_id) {
  ClientTrackerMap::iterator found = client_tracker_map_.find(client_id);
  if (found != client_tracker_map_.end())
    return found->second;
  return NULL;
}

void UsageTracker::GetGlobalUsage(const GlobalUsageCallback& callback) {
  if (client_tracker_map_.size() == 0) {
    // No clients registered.
    callback.Run(type_, 0, 0);
    return;
  }
  if (global_usage_callbacks_.Add(callback)) {
    // This is the first call.  Asks each ClientUsageTracker to collect
    // usage information.
    global_usage_.pending_clients = client_tracker_map_.size();
    global_usage_.usage = 0;
    global_usage_.unlimited_usage = 0;
    for (ClientTrackerMap::iterator iter = client_tracker_map_.begin();
         iter != client_tracker_map_.end();
         ++iter) {
      iter->second->GetGlobalUsage(
          base::Bind(&UsageTracker::DidGetClientGlobalUsage,
                     weak_factory_.GetWeakPtr()));
    }
  }
}

void UsageTracker::GetHostUsage(
    const std::string& host, const UsageCallback& callback) {
  if (client_tracker_map_.size() == 0) {
    // No clients registered.
    callback.Run(0);
    return;
  }
  if (host_usage_callbacks_.Add(host, callback)) {
    // This is the first call for the given host.
    DCHECK(outstanding_host_usage_.find(host) == outstanding_host_usage_.end());
    outstanding_host_usage_[host].pending_clients = client_tracker_map_.size();
    for (ClientTrackerMap::iterator iter = client_tracker_map_.begin();
         iter != client_tracker_map_.end();
         ++iter) {
      iter->second->GetHostUsage(host,
          base::Bind(&UsageTracker::DidGetClientHostUsage,
                     weak_factory_.GetWeakPtr(), host, type_));
    }
  }
}

void UsageTracker::UpdateUsageCache(
    QuotaClient::ID client_id, const GURL& origin, int64 delta) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);
  client_tracker->UpdateUsageCache(origin, delta);
}

void UsageTracker::GetCachedHostsUsage(
    std::map<std::string, int64>* host_usage) const {
  DCHECK(host_usage);
  host_usage->clear();
  for (ClientTrackerMap::const_iterator iter = client_tracker_map_.begin();
       iter != client_tracker_map_.end(); ++iter) {
    iter->second->GetCachedHostsUsage(host_usage);
  }
}

void UsageTracker::GetCachedOrigins(std::set<GURL>* origins) const {
  DCHECK(origins);
  origins->clear();
  for (ClientTrackerMap::const_iterator iter = client_tracker_map_.begin();
       iter != client_tracker_map_.end(); ++iter) {
    iter->second->GetCachedOrigins(origins);
  }
}

void UsageTracker::SetUsageCacheEnabled(QuotaClient::ID client_id,
                                        const GURL& origin,
                                        bool enabled) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);

  client_tracker->SetUsageCacheEnabled(origin, enabled);
}

void UsageTracker::DidGetClientGlobalUsage(StorageType type,
                                           int64 usage,
                                           int64 unlimited_usage) {
  DCHECK_EQ(type, type_);
  global_usage_.usage += usage;
  global_usage_.unlimited_usage += unlimited_usage;
  if (--global_usage_.pending_clients == 0) {
    // Defend against confusing inputs from clients.
    if (global_usage_.usage < 0)
      global_usage_.usage = 0;
    // TODO(michaeln): The unlimited number is not trustworthy, it
    // can get out of whack when apps are installed or uninstalled.
    if (global_usage_.unlimited_usage > global_usage_.usage)
      global_usage_.unlimited_usage = global_usage_.usage;
    else if (global_usage_.unlimited_usage < 0)
      global_usage_.unlimited_usage = 0;

    // All the clients have returned their usage data.  Dispatches the
    // pending callbacks.
    global_usage_callbacks_.Run(type, global_usage_.usage,
                                global_usage_.unlimited_usage);
  }
}

void UsageTracker::DidGetClientHostUsage(const std::string& host,
                                         StorageType type,
                                         int64 usage) {
  DCHECK_EQ(type, type_);
  TrackingInfo& info = outstanding_host_usage_[host];
  info.usage += usage;
  if (--info.pending_clients == 0) {
    // Defend against confusing inputs from clients.
    if (info.usage < 0)
      info.usage = 0;
    // All the clients have returned their usage data.  Dispatches the
    // pending callbacks.
    host_usage_callbacks_.Run(host, info.usage);
    outstanding_host_usage_.erase(host);
  }
}

// ClientUsageTracker ----------------------------------------------------

ClientUsageTracker::ClientUsageTracker(
    UsageTracker* tracker, QuotaClient* client, StorageType type,
    SpecialStoragePolicy* special_storage_policy)
    : tracker_(tracker),
      client_(client),
      type_(type),
      global_usage_(0),
      global_unlimited_usage_(0),
      global_usage_retrieved_(false),
      global_usage_task_(NULL),
      special_storage_policy_(special_storage_policy) {
  DCHECK(tracker_);
  DCHECK(client_);
  if (special_storage_policy_)
    special_storage_policy_->AddObserver(this);
}

ClientUsageTracker::~ClientUsageTracker() {
  if (special_storage_policy_)
    special_storage_policy_->RemoveObserver(this);
}

void ClientUsageTracker::GetGlobalUsage(const GlobalUsageCallback& callback) {
  if (global_usage_retrieved_ && non_cached_origins_by_host_.empty()) {
    callback.Run(type_, global_usage_, GetCachedGlobalUnlimitedUsage());
    return;
  }
  DCHECK(!global_usage_callback_.HasCallbacks());
  global_usage_callback_.Add(callback);
  global_usage_task_ = new GatherGlobalUsageTask(tracker_, client_);
  global_usage_task_->Start();
}

void ClientUsageTracker::GetHostUsage(
    const std::string& host, const UsageCallback& callback) {
  if (ContainsKey(cached_hosts_, host) &&
      !ContainsKey(non_cached_origins_by_host_, host)) {
    // TODO(kinuko): Drop host_usage_map_ cache periodically.
    callback.Run(GetCachedHostUsage(host));
    return;
  }
  if (!host_usage_callbacks_.Add(host, callback) || global_usage_task_)
    return;
  GatherHostUsageTask* task = new GatherHostUsageTask(tracker_, client_, host);
  host_usage_tasks_[host] = task;
  task->Start();
}

void ClientUsageTracker::UpdateUsageCache(
    const GURL& origin, int64 delta) {
  std::string host = net::GetHostOrSpecFromURL(origin);
  if (cached_hosts_.find(host) != cached_hosts_.end()) {
    if (!IsUsageCacheEnabledForOrigin(origin))
      return;

    cached_usage_by_host_[host][origin] += delta;
    global_usage_ += delta;
    if (IsStorageUnlimited(origin))
      global_unlimited_usage_ += delta;
    DCHECK_GE(cached_usage_by_host_[host][origin], 0);
    DCHECK_GE(global_usage_, 0);
    return;
  }

  // We don't know about this host yet, so populate our cache for it.
  GetHostUsage(host,
               base::Bind(&ClientUsageTracker::NoopHostUsageCallback,
                          base::Unretained(this)));
}

void ClientUsageTracker::GetCachedHostsUsage(
    std::map<std::string, int64>* host_usage) const {
  DCHECK(host_usage);
  for (HostUsageMap::const_iterator host_iter = cached_usage_by_host_.begin();
       host_iter != cached_usage_by_host_.end(); host_iter++) {
    host_usage->operator[](host_iter->first) +=
        GetCachedHostUsage(host_iter->first);
  }
}

void ClientUsageTracker::GetCachedOrigins(std::set<GURL>* origins) const {
  DCHECK(origins);
  for (HostUsageMap::const_iterator host_iter = cached_usage_by_host_.begin();
       host_iter != cached_usage_by_host_.end(); host_iter++) {
    const UsageMap& origin_map = host_iter->second;
    for (UsageMap::const_iterator origin_iter = origin_map.begin();
         origin_iter != origin_map.end(); origin_iter++) {
      origins->insert(origin_iter->first);
    }
  }
}

int64 ClientUsageTracker::GetCachedOriginsUsage(
    const std::set<GURL>& origins,
    std::vector<GURL>* origins_not_in_cache) {
  DCHECK(origins_not_in_cache);

  int64 usage = 0;
  for (std::set<GURL>::const_iterator itr = origins.begin();
       itr != origins.end(); ++itr) {
    int64 origin_usage = 0;
    if (GetCachedOriginUsage(*itr, &origin_usage))
      usage += origin_usage;
    else
      origins_not_in_cache->push_back(*itr);
  }
  return usage;
}

void ClientUsageTracker::SetUsageCacheEnabled(const GURL& origin,
                                              bool enabled) {
  std::string host = net::GetHostOrSpecFromURL(origin);
  if (!enabled) {
    // Erase |origin| from cache and subtract its usage.
    HostUsageMap::iterator found_host = cached_usage_by_host_.find(host);
    if (found_host != cached_usage_by_host_.end()) {
      UsageMap& cached_usage_for_host = found_host->second;

      UsageMap::iterator found = cached_usage_for_host.find(origin);
      if (found != cached_usage_for_host.end()) {
        int64 usage = found->second;
        UpdateUsageCache(origin, -usage);
        cached_usage_for_host.erase(found);
        if (cached_usage_for_host.empty()) {
          cached_usage_by_host_.erase(found_host);
          cached_hosts_.erase(host);
        }
      }
    }

    non_cached_origins_by_host_[host].insert(origin);
  } else {
    // Erase |origin| from |non_cached_origins_| and invalidate the usage cache
    // for the host.
    OriginSetByHost::iterator found = non_cached_origins_by_host_.find(host);
    if (found == non_cached_origins_by_host_.end())
      return;

    found->second.erase(origin);
    if (found->second.empty()) {
      non_cached_origins_by_host_.erase(found);
      cached_hosts_.erase(host);
      global_usage_retrieved_ = false;
    }
  }
}

void ClientUsageTracker::AddCachedOrigin(
    const GURL& origin, int64 usage) {
  if (!IsUsageCacheEnabledForOrigin(origin))
    return;

  std::string host = net::GetHostOrSpecFromURL(origin);
  UsageMap::iterator iter = cached_usage_by_host_[host].
      insert(UsageMap::value_type(origin, 0)).first;
  int64 old_usage = iter->second;
  iter->second = usage;
  int64 delta = usage - old_usage;
  if (delta) {
    global_usage_ += delta;
    if (IsStorageUnlimited(origin))
      global_unlimited_usage_ += delta;
  }
  DCHECK_GE(iter->second, 0);
  DCHECK_GE(global_usage_, 0);
}

void ClientUsageTracker::AddCachedHost(const std::string& host) {
  cached_hosts_.insert(host);
}

void ClientUsageTracker::GatherGlobalUsageComplete(
    int64 global_usage,
    int64 non_cached_global_usage,
    const std::map<std::string, int64>& non_cached_host_usage) {
  DCHECK(global_usage_task_ != NULL);
  global_usage_task_ = NULL;
  // TODO(kinuko): Record when it has retrieved the global usage.
  global_usage_retrieved_ = true;

  DCHECK(global_usage_callback_.HasCallbacks());
  global_usage_callback_.Run(
      type_, global_usage,
      GetCachedGlobalUnlimitedUsage() + non_cached_global_usage);

  for (HostUsageCallbackMap::iterator iter = host_usage_callbacks_.Begin();
       iter != host_usage_callbacks_.End(); ++iter) {
    int64 host_usage = GetCachedHostUsage(iter->first);
    std::map<std::string, int64>::const_iterator found =
        non_cached_host_usage.find(iter->first);
    if (found != non_cached_host_usage.end())
      host_usage += found->second;
    iter->second.Run(host_usage);
  }
  host_usage_callbacks_.Clear();
}

void ClientUsageTracker::GatherHostUsageComplete(const std::string& host,
                                                 int64 usage) {
  DCHECK(host_usage_tasks_.find(host) != host_usage_tasks_.end());
  host_usage_tasks_.erase(host);
  host_usage_callbacks_.Run(host, usage);
}

int64 ClientUsageTracker::GetCachedHostUsage(const std::string& host) const {
  HostUsageMap::const_iterator found = cached_usage_by_host_.find(host);
  if (found == cached_usage_by_host_.end())
    return 0;

  int64 usage = 0;
  const UsageMap& map = found->second;
  for (UsageMap::const_iterator iter = map.begin();
       iter != map.end(); ++iter) {
    usage += iter->second;
  }
  return usage;
}

int64 ClientUsageTracker::GetCachedGlobalUnlimitedUsage() {
  return global_unlimited_usage_;
}

bool ClientUsageTracker::GetCachedOriginUsage(
    const GURL& origin,
    int64* usage) const {
  std::string host = net::GetHostOrSpecFromURL(origin);
  HostUsageMap::const_iterator found_host = cached_usage_by_host_.find(host);
  if (found_host == cached_usage_by_host_.end())
    return false;

  UsageMap::const_iterator found = found_host->second.find(origin);
  if (found == found_host->second.end())
    return false;

  *usage = found->second;
  return true;
}

bool ClientUsageTracker::IsUsageCacheEnabledForOrigin(
    const GURL& origin) const {
  std::string host = net::GetHostOrSpecFromURL(origin);
  OriginSetByHost::const_iterator found =
      non_cached_origins_by_host_.find(host);
  return found == non_cached_origins_by_host_.end() ||
      !ContainsKey(found->second, origin);
}

void ClientUsageTracker::OnGranted(const GURL& origin,
                                   int change_flags) {
  DCHECK(CalledOnValidThread());
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64 usage = 0;
    if (GetCachedOriginUsage(origin, &usage))
      global_unlimited_usage_ += usage;
  }
}

void ClientUsageTracker::OnRevoked(const GURL& origin,
                                   int change_flags) {
  DCHECK(CalledOnValidThread());
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    int64 usage = 0;
    if (GetCachedOriginUsage(origin, &usage))
      global_unlimited_usage_ -= usage;
  }
}

void ClientUsageTracker::OnCleared() {
  DCHECK(CalledOnValidThread());
  global_unlimited_usage_ = 0;
}

void ClientUsageTracker::NoopHostUsageCallback(int64 usage) {}

bool ClientUsageTracker::IsStorageUnlimited(const GURL& origin) const {
  if (type_ == kStorageTypeSyncable)
    return false;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(origin);
}

}  // namespace quota
