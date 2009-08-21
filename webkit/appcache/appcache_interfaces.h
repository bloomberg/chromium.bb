// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_INTERFACES_H_
#define WEBKIT_APPCACHE_APPCACHE_INTERFACES_H_

#include <vector>
#include "base/basictypes.h"

class GURL;

namespace appcache {

// Defines constants, types, and abstract classes used in the main
// process and in child processes.

static const int kNoHostId = 0;
static const int64 kNoCacheId = 0;
static const int64 kUnknownCacheId = -1;

enum Status {
  UNCACHED,
  IDLE,
  CHECKING,
  DOWNLOADING,
  UPDATE_READY,
  OBSOLETE
};

enum EventID {
  CHECKING_EVENT,
  ERROR_EVENT,
  NO_UPDATE_EVENT,
  DOWNLOADING_EVENT,
  PROGRESS_EVENT,
  UPDATE_READY_EVENT,
  CACHED_EVENT,
  OBSOLETE_EVENT
};

// Interface used by backend to talk to frontend.
class AppCacheFrontend {
 public:
  virtual void OnCacheSelected(int host_id, int64 cache_id ,
                               Status status) = 0;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status) = 0;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id) = 0;

  virtual ~AppCacheFrontend() {}
};

// Interface used by frontend to talk to backend.
class AppCacheBackend {
 public:
  virtual void RegisterHost(int host_id) = 0;
  virtual void UnregisterHost(int host_id) = 0;
  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64 cache_document_was_loaded_from,
                           const GURL& manifest_url) = 0;
  virtual void MarkAsForeignEntry(int host_id, const GURL& document_url,
                                  int64 cache_document_was_loaded_from) = 0;
  virtual Status GetStatus(int host_id) = 0;
  virtual bool StartUpdate(int host_id) = 0;
  virtual bool SwapCache(int host_id) = 0;

  virtual ~AppCacheBackend() {}
};

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_INTERFACES_H_
