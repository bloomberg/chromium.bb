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
}

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
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
    DCHECK(tracker_);
    DCHECK(client_);
    client_tracker_ = tracker_->GetClientTracker(client_->id());
    DCHECK(client_tracker_);
  }
  virtual ~GatherUsageTaskBase() {}

  // Get total usage for the given |origins|.
  void GetUsageForOrigins(const std::set<GURL>& origins, StorageType type) {
    DCHECK(original_task_runner()->BelongsToCurrentThread());
    // We do not get usage for origins for which we have valid usage cache.
    std::vector<GURL> origins_to_gather;
    std::set<GURL> cached_origins;
    client_tracker()->GetCachedOrigins(&cached_origins);
    std::set<GURL> already_added;
    for (std::set<GURL>::const_iterator iter = origins.begin();
         iter != origins.end(); ++iter) {
      if (cached_origins.find(*iter) == cached_origins.end() &&
          already_added.insert(*iter).second) {
        origins_to_gather.push_back(*iter);
      }
    }
    if (origins_to_gather.empty()) {
      CallCompleted();
      DeleteSoon();
      return;
    }

    // Sort them so we can detect when we've gathered all info for a particular
    // host in DidGetUsage.
    std::sort(origins_to_gather.begin(), origins_to_gather.end(), SortByHost);

    // First, fully populate the pending queue because GetOriginUsage may call
    // the completion callback immediately.
    for (std::vector<GURL>::const_iterator iter = origins_to_gather.begin();
         iter != origins_to_gather.end(); iter++)
      pending_origins_.push_back(*iter);

    for (std::vector<GURL>::const_iterator iter = origins_to_gather.begin();
         iter != origins_to_gather.end(); iter++)
      client_->GetOriginUsage(
          *iter,
          tracker_->type(),
          base::Bind(&GatherUsageTaskBase::DidGetUsage,
                     weak_factory_.GetWeakPtr()));
  }

 protected:
  virtual void Aborted() OVERRIDE {
    DeleteSoon();
  }

  UsageTracker* tracker() const { return tracker_; }
  ClientUsageTracker* client_tracker() const { return client_tracker_; }

 private:
  void DidGetUsage(int64 usage) {
    DCHECK(original_task_runner()->BelongsToCurrentThread());
    DCHECK(!pending_origins_.empty());
    DCHECK(client_tracker_);

    // Defend against confusing inputs from QuotaClients.
    DCHECK_GE(usage, 0);
    if (usage < 0)
      usage = 0;

    // This code assumes DidGetUsage callbacks are called in the same
    // order as we dispatched GetOriginUsage calls.
    const GURL& origin = pending_origins_.front();
    std::string host = net::GetHostOrSpecFromURL(origin);
    client_tracker_->AddCachedOrigin(origin, usage);

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
  ClientUsageTracker* client_tracker_;
  std::deque<GURL> pending_origins_;
  std::map<GURL, int64> origin_usage_map_;
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
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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

  virtual void Completed() OVERRIDE {
    client_tracker()->GatherGlobalUsageComplete();
  }

 private:
  QuotaClient* client_;
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
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
    client_tracker()->GatherHostUsageComplete(host_);
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
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
    const std::string& host, const HostUsageCallback& callback) {
  if (client_tracker_map_.size() == 0) {
    // No clients registered.
    callback.Run(host, type_, 0);
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
                     weak_factory_.GetWeakPtr()));
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
    host_usage_callbacks_.Run(host, host, type, info.usage);
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
      global_unlimited_usage_is_valid_(true),
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
  if (global_usage_retrieved_) {
    callback.Run(type_, global_usage_, GetCachedGlobalUnlimitedUsage());
    return;
  }
  DCHECK(!global_usage_callback_.HasCallbacks());
  global_usage_callback_.Add(callback);
  global_usage_task_ = new GatherGlobalUsageTask(tracker_, client_);
  global_usage_task_->Start();
}

void ClientUsageTracker::GetHostUsage(
    const std::string& host, const HostUsageCallback& callback) {
  HostSet::const_iterator found = cached_hosts_.find(host);
  if (found != cached_hosts_.end()) {
    // TODO(kinuko): Drop host_usage_map_ cache periodically.
    callback.Run(host, type_, GetCachedHostUsage(host));
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
    cached_usage_[host][origin] += delta;
    global_usage_ += delta;
    if (global_unlimited_usage_is_valid_ && IsStorageUnlimited(origin))
      global_unlimited_usage_ += delta;
    DCHECK_GE(cached_usage_[host][origin], 0);
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
  for (HostUsageMap::const_iterator host_iter = cached_usage_.begin();
       host_iter != cached_usage_.end(); host_iter++) {
    host_usage->operator[](host_iter->first) +=
        GetCachedHostUsage(host_iter->first);
  }
}

void ClientUsageTracker::GetCachedOrigins(std::set<GURL>* origins) const {
  DCHECK(origins);
  for (HostUsageMap::const_iterator host_iter = cached_usage_.begin();
       host_iter != cached_usage_.end(); host_iter++) {
    const UsageMap& origin_map = host_iter->second;
    for (UsageMap::const_iterator origin_iter = origin_map.begin();
         origin_iter != origin_map.end(); origin_iter++) {
      origins->insert(origin_iter->first);
    }
  }
}

void ClientUsageTracker::AddCachedOrigin(
    const GURL& origin, int64 usage) {
  std::string host = net::GetHostOrSpecFromURL(origin);
  UsageMap::iterator iter = cached_usage_[host].
      insert(UsageMap::value_type(origin, 0)).first;
  int64 old_usage = iter->second;
  iter->second = usage;
  int64 delta = usage - old_usage;
  if (delta) {
    global_usage_ += delta;
    if (global_unlimited_usage_is_valid_ && IsStorageUnlimited(origin))
      global_unlimited_usage_ += delta;
  }
  DCHECK_GE(iter->second, 0);
  DCHECK_GE(global_usage_, 0);
}

void ClientUsageTracker::AddCachedHost(const std::string& host) {
  cached_hosts_.insert(host);
}

void ClientUsageTracker::GatherGlobalUsageComplete() {
  DCHECK(global_usage_task_ != NULL);
  global_usage_task_ = NULL;
  // TODO(kinuko): Record when it has retrieved the global usage.
  global_usage_retrieved_ = true;

  DCHECK(global_usage_callback_.HasCallbacks());
  global_usage_callback_.Run(type_, global_usage_,
                             GetCachedGlobalUnlimitedUsage());

  for (HostUsageCallbackMap::iterator iter = host_usage_callbacks_.Begin();
       iter != host_usage_callbacks_.End(); ++iter) {
    iter->second.Run(iter->first, type_, GetCachedHostUsage(iter->first));
  }
  host_usage_callbacks_.Clear();
}

void ClientUsageTracker::GatherHostUsageComplete(const std::string& host) {
  DCHECK(host_usage_tasks_.find(host) != host_usage_tasks_.end());
  host_usage_tasks_.erase(host);
  host_usage_callbacks_.Run(host, host, type_, GetCachedHostUsage(host));
}

int64 ClientUsageTracker::GetCachedHostUsage(const std::string& host) const {
  HostUsageMap::const_iterator found = cached_usage_.find(host);
  if (found == cached_usage_.end())
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
  if (!global_unlimited_usage_is_valid_) {
    global_unlimited_usage_ = 0;
    for (HostUsageMap::const_iterator host_iter = cached_usage_.begin();
         host_iter != cached_usage_.end(); host_iter++) {
      const UsageMap& origin_map = host_iter->second;
      for (UsageMap::const_iterator origin_iter = origin_map.begin();
           origin_iter != origin_map.end(); origin_iter++) {
        if (IsStorageUnlimited(origin_iter->first))
          global_unlimited_usage_ += origin_iter->second;
      }
    }
    global_unlimited_usage_is_valid_ = true;
  }
  return global_unlimited_usage_;
}

void ClientUsageTracker::OnSpecialStoragePolicyChanged() {
  DCHECK(CalledOnValidThread());
  global_unlimited_usage_is_valid_ = false;
}

void ClientUsageTracker::NoopHostUsageCallback(
    const std::string& host,  StorageType type, int64 usage) {
}

bool ClientUsageTracker::IsStorageUnlimited(const GURL& origin) const {
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(origin);
}

}  // namespace quota
