// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_INTERFACES_H_
#define WEBKIT_APPCACHE_APPCACHE_INTERFACES_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "webkit/storage/webkit_storage_export.h"

namespace net {
class URLRequest;
}  // namespace net

namespace appcache {

// Defines constants, types, and abstract classes used in the main
// process and in child processes.

static const int kNoHostId = 0;
static const int64 kNoCacheId = 0;
static const int64 kNoResponseId = 0;
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

// Temporarily renumber them in wierd way, to help remove LOG_TIP from WebKit
enum LogLevel {
  LOG_DEBUG = 4,
  LOG_INFO = 1,
  LOG_WARNING = 2,
  LOG_ERROR = 3,
};

enum NamespaceType {
  FALLBACK_NAMESPACE,
  INTERCEPT_NAMESPACE
};

struct WEBKIT_STORAGE_EXPORT AppCacheInfo {
  AppCacheInfo();
  ~AppCacheInfo();

  GURL manifest_url;
  base::Time creation_time;
  base::Time last_update_time;
  base::Time last_access_time;
  int64 cache_id;
  int64 group_id;
  Status status;
  int64 size;
  bool is_complete;
};

typedef std::vector<AppCacheInfo> AppCacheInfoVector;

// Type to hold information about a single appcache resource.
struct WEBKIT_STORAGE_EXPORT AppCacheResourceInfo {
  AppCacheResourceInfo();
  ~AppCacheResourceInfo();

  GURL url;
  int64 size;
  bool is_master;
  bool is_manifest;
  bool is_intercept;
  bool is_fallback;
  bool is_foreign;
  bool is_explicit;
  int64 response_id;
};

typedef std::vector<AppCacheResourceInfo> AppCacheResourceInfoVector;

struct WEBKIT_STORAGE_EXPORT Namespace {
  Namespace();  // Type is set to FALLBACK_NAMESPACE by default.
  Namespace(NamespaceType type, const GURL& url, const GURL& target);
  ~Namespace();

  NamespaceType type;
  GURL namespace_url;
  GURL target_url;
};

typedef std::vector<Namespace> NamespaceVector;

// Interface used by backend (browser-process) to talk to frontend (renderer).
class WEBKIT_STORAGE_EXPORT AppCacheFrontend {
 public:
  virtual void OnCacheSelected(
      int host_id, const appcache::AppCacheInfo& info) = 0;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status) = 0;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id) = 0;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) = 0;
  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const std::string& message) = 0;
  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url) = 0;
  virtual void OnLogMessage(int host_id, LogLevel log_level,
                            const std::string& message) = 0;
  virtual ~AppCacheFrontend() {}
};

// Interface used by frontend (renderer) to talk to backend (browser-process).
class WEBKIT_STORAGE_EXPORT AppCacheBackend {
 public:
  virtual void RegisterHost(int host_id) = 0;
  virtual void UnregisterHost(int host_id) = 0;
  virtual void SetSpawningHostId(int host_id, int spawning_host_id) = 0;
  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64 cache_document_was_loaded_from,
                           const GURL& manifest_url) = 0;
  virtual void SelectCacheForWorker(
                           int host_id,
                           int parent_process_id,
                           int parent_host_id) = 0;
  virtual void SelectCacheForSharedWorker(
                           int host_id,
                           int64 appcache_id) = 0;
  virtual void MarkAsForeignEntry(int host_id, const GURL& document_url,
                                  int64 cache_document_was_loaded_from) = 0;
  virtual Status GetStatus(int host_id) = 0;
  virtual bool StartUpdate(int host_id) = 0;
  virtual bool SwapCache(int host_id) = 0;
  virtual void GetResourceList(
      int host_id, std::vector<AppCacheResourceInfo>* resource_infos) = 0;

 protected:
  virtual ~AppCacheBackend() {}
};

// Useful string constants.
// Note: These are also defined elsewhere in the chrome code base in
// url_contants.h .cc, however the appcache library doesn't not have
// any dependencies on the chrome library, so we can't use them in here.
extern const char kHttpScheme[];
extern const char kHttpsScheme[];
extern const char kHttpGETMethod[];
extern const char kHttpHEADMethod[];

bool IsSchemeSupported(const GURL& url);
bool IsMethodSupported(const std::string& method);
bool IsSchemeAndMethodSupported(const net::URLRequest* request);

WEBKIT_STORAGE_EXPORT extern const base::FilePath::CharType
    kAppCacheDatabaseName[];

}  // namespace

#endif  // WEBKIT_APPCACHE_APPCACHE_INTERFACES_H_
