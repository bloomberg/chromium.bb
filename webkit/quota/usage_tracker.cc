// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/usage_tracker.h"

#include <deque>
#include <set>
#include <string>

#include "base/message_loop_proxy.h"
#include "base/stl_util-inl.h"
#include "net/base/net_util.h"

namespace quota {

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
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
    DCHECK(tracker_);
    DCHECK(client_);
    client_tracker_ = tracker_->GetClientTracker(client_->id());
    DCHECK(client_tracker_);
  }
  virtual ~GatherUsageTaskBase() {}

  // Get total usage for the given |origins|.
  void GetUsageForOrigins(const std::set<GURL>& origins) {
    DCHECK(original_message_loop()->BelongsToCurrentThread());
    std::set<GURL> origins_to_process;
    // We do not get usage for origins for which we have valid usage cache.
    client_tracker()->DetermineOriginsToGetUsage(origins, &origins_to_process);
    if (origins_to_process.empty()) {
      // Nothing to be done.
      CallCompleted();
      DeleteSoon();
    }
    for (std::set<GURL>::const_iterator iter = origins_to_process.begin();
         iter != origins_to_process.end();
         iter++) {
      pending_origins_.push_back(*iter);
      client_->GetOriginUsage(
          *iter,
          tracker_->type(),
          callback_factory_.NewCallback(&GatherUsageTaskBase::DidGetUsage));
    }
  }

  bool IsOriginDone(const GURL& origin) const {
    DCHECK(original_message_loop()->BelongsToCurrentThread());
    return origin_usage_map_.find(origin) != origin_usage_map_.end();
  }

 protected:
  virtual void Aborted() OVERRIDE {
    DeleteSoon();
  }

  UsageTracker* tracker() const { return tracker_; }
  ClientUsageTracker* client_tracker() const { return client_tracker_; }
  const std::map<GURL, int64>& origin_usage_map() const {
    return origin_usage_map_;
  }

 private:
  void DidGetUsage(int64 usage) {
    // This code assumes DidGetUsage callbacks are called in the same
    // order as we dispatched GetOriginUsage calls.
    DCHECK(original_message_loop()->BelongsToCurrentThread());
    DCHECK(!pending_origins_.empty());
    origin_usage_map_.insert(std::make_pair(
        pending_origins_.front(), usage));
    pending_origins_.pop_front();
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
  base::ScopedCallbackFactory<GatherUsageTaskBase> callback_factory_;

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
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
    DCHECK(tracker);
    DCHECK(client);
  }
  virtual ~GatherGlobalUsageTask() {}

 protected:
  virtual void Run() OVERRIDE {
    client_->GetOriginsForType(tracker()->type(),
          callback_factory_.NewCallback(
              &GatherUsageTaskBase::GetUsageForOrigins));
  }

  virtual void Completed() OVERRIDE {
    client_tracker()->DidGetGlobalUsage(origin_usage_map());
  }

 private:
  QuotaClient* client_;
  base::ScopedCallbackFactory<GatherUsageTaskBase> callback_factory_;

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
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
    DCHECK(client_);
  }
  virtual ~GatherHostUsageTask() {}

 protected:
  virtual void Run() OVERRIDE {
    client_->GetOriginsForHost(tracker()->type(), host_,
          callback_factory_.NewCallback(
              &GatherUsageTaskBase::GetUsageForOrigins));
  }

  virtual void Completed() OVERRIDE {
    client_tracker()->DidGetHostUsage(host_, origin_usage_map());
  }

 private:
  QuotaClient* client_;
  std::string host_;
  base::ScopedCallbackFactory<GatherUsageTaskBase> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(GatherHostUsageTask);
};

// UsageTracker ----------------------------------------------------------

UsageTracker::UsageTracker(const QuotaClientList& clients, StorageType type)
    : type_(type),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  for (QuotaClientList::const_iterator iter = clients.begin();
      iter != clients.end();
      ++iter) {
    client_tracker_map_.insert(std::make_pair(
        (*iter)->id(), new ClientUsageTracker(this, *iter)));
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

void UsageTracker::GetGlobalUsage(UsageCallback* callback) {
  if (client_tracker_map_.size() == 0) {
    // No clients registered.
    callback->Run(0);
    delete callback;
    return;
  }
  if (global_usage_callbacks_.Add(callback)) {
    // This is the first call.  Asks each ClientUsageTracker to collect
    // usage information.
    global_usage_.pending_clients = client_tracker_map_.size();
    global_usage_.usage = 0;
    for (ClientTrackerMap::iterator iter = client_tracker_map_.begin();
         iter != client_tracker_map_.end();
         ++iter) {
      iter->second->GetGlobalUsage(callback_factory_.NewCallback(
          &UsageTracker::DidGetClientGlobalUsage));
    }
  }
}

void UsageTracker::GetHostUsage(
    const std::string& host, HostUsageCallback* callback) {
  if (client_tracker_map_.size() == 0) {
    // No clients registered.
    callback->Run(host, 0);
    delete callback;
    return;
  }
  if (host_usage_callbacks_.Add(host, callback)) {
    // This is the first call for the given host.
    DCHECK(outstanding_host_usage_.find(host) == outstanding_host_usage_.end());
    outstanding_host_usage_[host].pending_clients = client_tracker_map_.size();
    for (ClientTrackerMap::iterator iter = client_tracker_map_.begin();
         iter != client_tracker_map_.end();
         ++iter) {
      iter->second->GetHostUsage(host, callback_factory_.NewCallback(
          &UsageTracker::DidGetClientHostUsage));
    }
  }
}

void UsageTracker::UpdateUsageCache(
    QuotaClient::ID client_id, const GURL& origin, int64 delta) {
  ClientUsageTracker* client_tracker = GetClientTracker(client_id);
  DCHECK(client_tracker);
  client_tracker->UpdateUsageCache(origin, delta);
}

void UsageTracker::DidGetClientGlobalUsage(int64 usage) {
  global_usage_.usage += usage;
  if (--global_usage_.pending_clients == 0) {
    // All the clients have returned their usage data.  Dispatches the
    // pending callbacks.
    global_usage_callbacks_.Run(global_usage_.usage);
  }
}

void UsageTracker::DidGetClientHostUsage(const std::string& host, int64 usage) {
  TrackingInfo& info = outstanding_host_usage_[host];
  info.usage += usage;
  if (--info.pending_clients == 0) {
    // All the clients have returned their usage data.  Dispatches the
    // pending callbacks.
    host_usage_callbacks_.Run(host, host, info.usage);
    outstanding_host_usage_.erase(host);
  }
}

// ClientUsageTracker ----------------------------------------------------

ClientUsageTracker::ClientUsageTracker(
    UsageTracker* tracker, QuotaClient* client)
    : tracker_(tracker),
      client_(client),
      global_usage_(0),
      global_usage_retrieved_(false),
      global_usage_task_(NULL) {
  DCHECK(tracker_);
  DCHECK(client_);
}

ClientUsageTracker::~ClientUsageTracker() {
}

void ClientUsageTracker::GetGlobalUsage(UsageCallback* callback) {
  if (global_usage_retrieved_) {
    callback->Run(global_usage_);
    delete callback;
    return;
  }
  DCHECK(!global_usage_callback_.HasCallbacks());
  global_usage_callback_.Add(callback);
  global_usage_task_ = new GatherGlobalUsageTask(tracker_, client_);
  global_usage_task_->Start();
}

void ClientUsageTracker::GetHostUsage(
    const std::string& host, HostUsageCallback* callback) {
  std::map<std::string, int64>::iterator found = host_usage_map_.find(host);
  if (found != host_usage_map_.end()) {
    // TODO(kinuko): Drop host_usage_map_ cache periodically.
    callback->Run(host, found->second);
    delete callback;
    return;
  }
  DCHECK(!host_usage_callbacks_.HasCallbacks(host));
  DCHECK(host_usage_tasks_.find(host) == host_usage_tasks_.end());
  host_usage_callbacks_.Add(host, callback);
  if (global_usage_task_)
    return;
  GatherHostUsageTask* task = new GatherHostUsageTask(tracker_, client_, host);
  task->Start();
  host_usage_tasks_[host] = task;
}

void ClientUsageTracker::DetermineOriginsToGetUsage(
    const std::set<GURL>& origins, std::set<GURL>* origins_to_process) {
  DCHECK(origins_to_process);
  for (std::set<GURL>::const_iterator iter = origins.begin();
       iter != origins.end(); ++iter) {
    if (cached_origins_.find(*iter) == cached_origins_.end())
      origins_to_process->insert(*iter);
  }
}

void ClientUsageTracker::UpdateUsageCache(
    const GURL& origin, int64 delta) {
  std::string host = net::GetHostOrSpecFromURL(origin);
  if (cached_origins_.find(origin) != cached_origins_.end()) {
    host_usage_map_[host] += delta;
    global_usage_ += delta;
    return;
  }
  if (global_usage_retrieved_ ||
      host_usage_map_.find(host) != host_usage_map_.end()) {
    // This might be for a new origin.
    cached_origins_.insert(origin);
    host_usage_map_[host] = delta;
    global_usage_ += delta;
    return;
  }
  // See if the origin has been processed in outstanding gather tasks
  // and add up the delta if it has.
  if (global_usage_task_ && global_usage_task_->IsOriginDone(origin)) {
    host_usage_map_[host] += delta;
    global_usage_ += delta;
    return;
  }
  if (host_usage_tasks_.find(host) != host_usage_tasks_.end() &&
      host_usage_tasks_[host]->IsOriginDone(origin)) {
    host_usage_map_[host] += delta;
  }
  // Otherwise we have not cached usage info for the origin yet.
  // Succeeding GetUsage tasks would eventually catch the change.
}

void ClientUsageTracker::DidGetGlobalUsage(
    const std::map<GURL, int64>& origin_usage_map) {
  DCHECK(global_usage_task_ != NULL);
  global_usage_task_ = NULL;
  // TODO(kinuko): Record when it has retrieved the global usage.
  global_usage_retrieved_ = true;
  for (std::map<GURL, int64>::const_iterator iter = origin_usage_map.begin();
       iter != origin_usage_map.end();
       ++iter) {
    if (cached_origins_.insert(iter->first).second) {
      global_usage_ += iter->second;
      std::string host = net::GetHostOrSpecFromURL(iter->first);
      host_usage_map_[host] += iter->second;
    }
  }

  // Dispatches the global usage callback.
  DCHECK(global_usage_callback_.HasCallbacks());
  global_usage_callback_.Run(global_usage_);

  // Dispatches host usage callbacks.
  for (HostUsageCallbackMap::iterator iter = host_usage_callbacks_.Begin();
       iter != host_usage_callbacks_.End();
       ++iter) {
    std::map<std::string, int64>::iterator found  =
        host_usage_map_.find(iter->first);
    if (found == host_usage_map_.end())
      iter->second.Run(iter->first, 0);
    else
      iter->second.Run(iter->first, found->second);
  }
  host_usage_callbacks_.Clear();
}

void ClientUsageTracker::DidGetHostUsage(
    const std::string& host,
    const std::map<GURL, int64>& origin_usage_map) {
  DCHECK(host_usage_tasks_.find(host) != host_usage_tasks_.end());
  host_usage_tasks_.erase(host);
  for (std::map<GURL, int64>::const_iterator iter = origin_usage_map.begin();
       iter != origin_usage_map.end();
       ++iter) {
    if (cached_origins_.insert(iter->first).second) {
      global_usage_ += iter->second;
      host_usage_map_[host] += iter->second;
    }
  }

  // Dispatches the host usage callback.
  host_usage_callbacks_.Run(host, host, host_usage_map_[host]);
}

}  // namespace quota
