// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_USAGE_TRACKER_H_
#define WEBKIT_QUOTA_USAGE_TRACKER_H_
#pragma once

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

namespace quota {

class ClientUsageTracker;

// A helper class that gathers and tracks the amount of data stored in
// all quota clients.
// An instance of this class is created per storage type.
class UsageTracker : public QuotaTaskObserver {
 public:
  UsageTracker(const QuotaClientList& clients, StorageType type);
  ~UsageTracker();

  StorageType type() const { return type_; }
  ClientUsageTracker* GetClientTracker(QuotaClient::ID client_id);

  void GetGlobalUsage(UsageCallback* callback);
  void GetHostUsage(const std::string& host, HostUsageCallback* callback);
  void UpdateUsageCache(QuotaClient::ID client_id,
                        const GURL& origin,
                        int64 delta);

  void GetCachedOrigins(std::set<GURL>* origins) const;

 private:
  struct TrackingInfo {
    TrackingInfo() : pending_clients(0), usage(0) {}
    int pending_clients;
    int64 usage;
  };

  typedef std::map<QuotaClient::ID, ClientUsageTracker*> ClientTrackerMap;

  friend class ClientUsageTracker;
  void DidGetClientGlobalUsage(int64 usage);
  void DidGetClientHostUsage(const std::string& host, int64 usage);

  StorageType type_;
  ClientTrackerMap client_tracker_map_;
  TrackingInfo global_usage_;
  std::map<std::string, TrackingInfo> outstanding_host_usage_;

  UsageCallbackQueue global_usage_callbacks_;
  HostUsageCallbackMap host_usage_callbacks_;

  base::ScopedCallbackFactory<UsageTracker> callback_factory_;
  DISALLOW_COPY_AND_ASSIGN(UsageTracker);
};

// This class holds per-client usage tracking information and caches per-host
// usage data.  An instance of this class is created per client.
class ClientUsageTracker {
 public:
  ClientUsageTracker(UsageTracker* tracking_info, QuotaClient* client);
  ~ClientUsageTracker();

  void GetGlobalUsage(UsageCallback* callback);
  void GetHostUsage(const std::string& host, HostUsageCallback* callback);
  void DetermineOriginsToGetUsage(const std::set<GURL>& origins,
                                  std::set<GURL>* origins_to_process);
  void UpdateUsageCache(const GURL& origin, int64 delta);

  const std::set<GURL>& cached_origins() const { return cached_origins_; }

 private:
  class GatherUsageTaskBase;
  class GatherGlobalUsageTask;
  class GatherHostUsageTask;

  void DidGetGlobalUsage(const std::map<GURL, int64>& origin_usage_map);
  void DidGetHostUsage(const std::string& host,
                       const std::map<GURL, int64>& origin_usage_map);

  UsageTracker* tracker_;
  QuotaClient* client_;
  std::set<GURL> cached_origins_;

  int64 global_usage_;
  bool global_usage_retrieved_;
  GatherGlobalUsageTask* global_usage_task_;
  UsageCallbackQueue global_usage_callback_;

  std::map<std::string, int64> host_usage_map_;
  std::map<std::string, GatherHostUsageTask*> host_usage_tasks_;
  HostUsageCallbackMap host_usage_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ClientUsageTracker);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_USAGE_TRACKER_H_
