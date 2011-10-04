// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_USAGE_TRACKER_H_
#define WEBKIT_QUOTA_USAGE_TRACKER_H_
#pragma once

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"
#include "webkit/quota/special_storage_policy.h"

namespace quota {

class ClientUsageTracker;

// A helper class that gathers and tracks the amount of data stored in
// all quota clients.
// An instance of this class is created per storage type.
class UsageTracker : public QuotaTaskObserver {
 public:
  UsageTracker(const QuotaClientList& clients, StorageType type,
               SpecialStoragePolicy* special_storage_policy);
  virtual ~UsageTracker();

  StorageType type() const { return type_; }
  ClientUsageTracker* GetClientTracker(QuotaClient::ID client_id);

  void GetGlobalUsage(GlobalUsageCallback* callback);
  void GetHostUsage(const std::string& host, HostUsageCallback* callback);
  void UpdateUsageCache(QuotaClient::ID client_id,
                        const GURL& origin,
                        int64 delta);
  void GetCachedHostsUsage(std::map<std::string, int64>* host_usage) const;
  void GetCachedOrigins(std::set<GURL>* origins) const;
  bool IsWorking() const {
    return global_usage_callbacks_.HasCallbacks() ||
           host_usage_callbacks_.HasAnyCallbacks();
  }

 private:
  struct TrackingInfo {
    TrackingInfo() : pending_clients(0), usage(0), unlimited_usage(0) {}
    int pending_clients;
    int64 usage;
    int64 unlimited_usage;
  };

  typedef std::map<QuotaClient::ID, ClientUsageTracker*> ClientTrackerMap;

  friend class ClientUsageTracker;
  void DidGetClientGlobalUsage(StorageType type, int64 usage,
                               int64 unlimited_usage);
  void DidGetClientHostUsage(const std::string& host,
                             StorageType type,
                             int64 usage);

  const StorageType type_;
  ClientTrackerMap client_tracker_map_;
  TrackingInfo global_usage_;
  std::map<std::string, TrackingInfo> outstanding_host_usage_;

  GlobalUsageCallbackQueue global_usage_callbacks_;
  HostUsageCallbackMap host_usage_callbacks_;

  base::ScopedCallbackFactory<UsageTracker> callback_factory_;
  DISALLOW_COPY_AND_ASSIGN(UsageTracker);
};

// This class holds per-client usage tracking information and caches per-host
// usage data.  An instance of this class is created per client.
class ClientUsageTracker : public SpecialStoragePolicy::Observer,
                           public base::NonThreadSafe {
 public:
  ClientUsageTracker(UsageTracker* tracker,
                     QuotaClient* client,
                     StorageType type,
                     SpecialStoragePolicy* special_storage_policy);
  virtual ~ClientUsageTracker();

  void GetGlobalUsage(GlobalUsageCallback* callback);
  void GetHostUsage(const std::string& host, HostUsageCallback* callback);
  void UpdateUsageCache(const GURL& origin, int64 delta);
  void GetCachedHostsUsage(std::map<std::string, int64>* host_usage) const;
  void GetCachedOrigins(std::set<GURL>* origins) const;

 private:
  typedef std::set<std::string> HostSet;
  typedef std::map<GURL, int64> UsageMap;
  typedef std::map<std::string, UsageMap> HostUsageMap;

  class GatherUsageTaskBase;
  class GatherGlobalUsageTask;
  class GatherHostUsageTask;

  // Methods used by our GatherUsage tasks, as a task makes progress
  // origins and hosts are added incrementally to the cache.
  void AddCachedOrigin(const GURL& origin, int64 usage);
  void AddCachedHost(const std::string& host);
  void GatherGlobalUsageComplete();
  void GatherHostUsageComplete(const std::string& host);

  int64 GetCachedHostUsage(const std::string& host) const;
  int64 GetCachedGlobalUnlimitedUsage();
  virtual void OnSpecialStoragePolicyChanged() OVERRIDE;
  void NoopHostUsageCallback(
    const std::string& host,  StorageType type, int64 usage);
  bool IsStorageUnlimited(const GURL& origin) const;

  UsageTracker* tracker_;
  QuotaClient* client_;
  const StorageType type_;

  int64 global_usage_;
  int64 global_unlimited_usage_;
  bool global_usage_retrieved_;
  bool global_unlimited_usage_is_valid_;
  HostSet cached_hosts_;
  HostUsageMap cached_usage_;

  GatherGlobalUsageTask* global_usage_task_;
  GlobalUsageCallbackQueue global_usage_callback_;
  std::map<std::string, GatherHostUsageTask*> host_usage_tasks_;
  HostUsageCallbackMap host_usage_callbacks_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(ClientUsageTracker);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_USAGE_TRACKER_H_
