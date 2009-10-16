// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_UPDATE_JOB_H_
#define WEBKIT_APPCACHE_APPCACHE_UPDATE_JOB_H_

#include <deque>
#include <map>

#include "base/ref_counted.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_interfaces.h"

namespace appcache {

struct UpdateJobInfo;

// Application cache Update algorithm and state.
class AppCacheUpdateJob : public URLRequest::Delegate {
 public:
  AppCacheUpdateJob(AppCacheService* service, AppCacheGroup* group);
  ~AppCacheUpdateJob();

  // Triggers the update process or adds more info if this update is already
  // in progress.
  void StartUpdate(AppCacheHost* host, const GURL& new_master_resource);


  // TODO(jennb): add callback method for writing data to storage
  // TODO(jennb): add callback method for reading data from storage

 private:
  friend class ScopedRunnableMethodFactory<AppCacheUpdateJob>;
  friend class AppCacheUpdateJobTest;

  // Master entries have multiple hosts, for example, the same page is opened
  // in different tabs.
  // TODO(jennb): detect when hosts are deleted
  typedef std::vector<AppCacheHost*> PendingHosts;
  typedef std::map<GURL, PendingHosts> PendingMasters;
  typedef std::map<GURL, URLRequest*> PendingUrlFetches;
  typedef std::pair<GURL, bool> UrlToFetch;  // flag TRUE if storage checked

  static const int kRerunDelayMs = 1000;

  enum UpdateType {
    UNKNOWN_TYPE,
    UPGRADE_ATTEMPT,
    CACHE_ATTEMPT,
  };

  enum InternalUpdateState {
    FETCH_MANIFEST,
    NO_UPDATE,
    DOWNLOADING,
    REFETCH_MANIFEST,
    CACHE_FAILURE,
    CANCELLED,
    COMPLETED,
  };

  // Methods for URLRequest::Delegate.
  void OnResponseStarted(URLRequest* request);
  void OnReadCompleted(URLRequest* request, int bytes_read);
  void OnReceivedRedirect(URLRequest* request,
                          const GURL& new_url,
                          bool* defer_redirect);
  // TODO(jennb): any other delegate callbacks to handle? certificate?

  void FetchManifest(bool is_first_fetch);

  void OnResponseCompleted(URLRequest* request);

  // Retries a 503 request with retry-after header of 0.
  // Returns true if request should be retried and deletes original request.
  bool RetryRequest(URLRequest* request);

  void ReadResponseData(URLRequest* request);

  // Returns false if response data is processed asynchronously, in which
  // case a callback will be invoked when it is safe to continue reading
  // more response data from the request.
  bool ConsumeResponseData(URLRequest* request,
                           UpdateJobInfo* info,
                           int bytes_read);

  void HandleManifestFetchCompleted(URLRequest* request);
  void ContinueHandleManifestFetchCompleted(bool changed);
  void HandleUrlFetchCompleted(URLRequest* request);
  void HandleManifestRefetchCompleted(URLRequest* request);

  void NotifySingleHost(AppCacheHost* host, EventID event_id);
  void NotifyAllPendingMasterHosts(EventID event_id);
  void NotifyAllAssociatedHosts(EventID event_id);

  // Checks if manifest is byte for byte identical with the manifest
  // in the newest application cache.
  void CheckIfManifestChanged();
  void ContinueCheckIfManifestChanged(const std::string& loaded_manifest);

  // TODO(jennb): delete when able to mock storage behavior
  void SimulateManifestChanged(bool changed) {
    simulate_manifest_changed_ = changed;
  }

  // Creates the list of files that may need to be fetched and initiates
  // fetches. Section 6.9.4 steps 12-17
  void BuildUrlFileList(const Manifest& manifest);
  void AddUrlToFileList(const GURL& url, int type);
  void FetchUrls();
  bool ShouldSkipUrlFetch(const AppCacheEntry& entry);

  // Asynchronously loads the entry from the newest complete cache if the
  // HTTP caching semantics allow.
  // Returns false if immediately obvious that data cannot be loaded from
  // newest complete cache.
  bool MaybeLoadFromNewestCache(const GURL& url, AppCacheEntry& entry);

  // TODO(jennb): delete me after storage code added
  void SimulateFailedLoadFromNewestCache(const GURL& url);

  // Copies the data from src entry to dest entry and adds the modified
  // entry to the inprogress cache.
  void CopyEntryToCache(const GURL& url, const AppCacheEntry& src,
                        AppCacheEntry* dest);

  // Does nothing if update process is still waiting for pending master
  // entries or URL fetches to complete downloading. Otherwise, completes
  // the update process.
  void MaybeCompleteUpdate();

  // Schedules a rerun of the entire update with the same parameters as
  // this update job after a short delay.
  void ScheduleUpdateRetry(int delay_ms);

  void Cancel();
  void DiscardInprogressCache();

  // Deletes this object after letting the stack unwind.
  void DeleteSoon();

  // This factory will be used to schedule invocations of various methods.
  ScopedRunnableMethodFactory<AppCacheUpdateJob> method_factory_;

  GURL manifest_url_;  // here for easier access
  AppCacheService* service_;

  scoped_refptr<AppCache> inprogress_cache_;

  // Hold a reference to the newly created cache until this update is
  // destroyed. The host that started the update will add a reference to the
  // new cache when notified that the update is complete. AppCacheGroup sends
  // the notification when its status is set to IDLE in ~AppCacheUpdateJob.
  scoped_refptr<AppCache> protect_new_cache_;

  AppCacheGroup* group_;

  UpdateType update_type_;
  InternalUpdateState internal_state_;

  PendingMasters pending_master_entries_;
  size_t master_entries_completed_;

  // URLs of files to fetch along with their flags.
  AppCache::EntryMap url_file_list_;
  size_t url_fetches_completed_;

  // Helper container to track which urls have not been fetched yet. URLs are
  // removed when the fetch is initiated. Flag indicates whether an attempt
  // to load the URL from storage has already been tried and failed.
  std::deque<UrlToFetch> urls_to_fetch_;

  // Keep track of pending URL requests so we can cancel them if necessary.
  URLRequest* manifest_url_request_;
  PendingUrlFetches pending_url_fetches_;

  // Temporary storage of manifest response data for parsing and comparison.
  std::string manifest_data_;
  std::string manifest_refetch_data_;

  // TODO(jennb): delete when able to mock storage behavior
  bool simulate_manifest_changed_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheUpdateJob);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_UPDATE_JOB_H_
