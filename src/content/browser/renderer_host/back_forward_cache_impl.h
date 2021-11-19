// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_

#include <list>
#include <memory>
#include <set>
#include <unordered_set>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_process_host_internal_observer.h"
#include "content/browser/renderer_host/stored_page.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;
class RenderViewHostImpl;
class SiteInstance;

// This feature is used to limit the scope of back-forward cache experiment
// without enabling it. To control the URLs list by using this feature by
// generating the metrics only for "allowed_websites" param. Mainly, to ensure
// that metrics from the control and experiment groups are consistent.
constexpr base::Feature kRecordBackForwardCacheMetricsWithoutEnabling{
    "RecordBackForwardCacheMetricsWithoutEnabling",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Removes the time limit for cached content. This is used on bots to identify
// accidentally passing tests.
constexpr base::Feature kBackForwardCacheNoTimeEviction{
    "BackForwardCacheNoTimeEviction", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows pages with cache-control:no-store to enter the back/forward cache.
// Feature params can specify whether pages with cache-control:no-store can be
// restored if cookies change / if HTTPOnly cookies change.
// TODO(crbug.com/1228611): Enable this feature.
const base::Feature kCacheControlNoStoreEnterBackForwardCache{
    "CacheControlNoStoreEnterBackForwardCache",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Allows pages with MediaSession's playback state change to stay eligible for
// the back/forward cache.
const base::Feature kBackForwardCacheMediaSessionPlaybackStateChange{
    "BackForwardCacheMediaSessionPlaybackStateChange",
    base::FEATURE_DISABLED_BY_DEFAULT};

// BackForwardCache:
//
// After the user navigates away from a document, the old one goes into the
// frozen state and is kept in this object. They can potentially be reused
// after an history navigation. Reusing a document means swapping it back with
// the current_frame_host.
class CONTENT_EXPORT BackForwardCacheImpl
    : public BackForwardCache,
      public RenderProcessHostInternalObserver {
 public:
  enum MessageHandlingPolicyWhenCached {
    kMessagePolicyNone,
    kMessagePolicyLog,
    kMessagePolicyDump,
  };

  static MessageHandlingPolicyWhenCached
  GetChannelAssociatedMessageHandlingPolicy();

  // BackForwardCache entry, consisting of the page and associated metadata.
  class Entry : public ::network::mojom::CookieChangeListener {
   public:
    explicit Entry(std::unique_ptr<StoredPage> stored_page);
    ~Entry() override;

    void WriteIntoTrace(perfetto::TracedValue context);

    // Starts monitoring the cookie change in this entry.
    void StartMonitoringCookieChange();

    // Indicates whether or not all the |render_view_hosts| in this entry have
    // received the acknowledgement from renderer that it finished running
    // handlers.
    bool AllRenderViewHostsReceivedAckFromRenderer();

    std::unique_ptr<StoredPage> TakeStoredPage() {
      return std::move(stored_page_);
    }
    void SetPageRestoreParams(
        blink::mojom::PageRestoreParamsPtr page_restore_params) {
      stored_page_->page_restore_params = std::move(page_restore_params);
    }

    // The main document being stored.
    RenderFrameHostImpl* render_frame_host() {
      return stored_page_->render_frame_host.get();
    }

    std::set<RenderViewHostImpl*> render_view_hosts() {
      return stored_page_->render_view_hosts;
    }

    const StoredPage::RenderFrameProxyHostMap& proxy_hosts() const {
      return stored_page_->proxy_hosts;
    }

    size_t proxy_hosts_size() { return stored_page_->proxy_hosts.size(); }

   private:
    friend class BackForwardCacheImpl;

    // ::network::mojom::CookieChangeListener
    void OnCookieChange(const net::CookieChangeInfo& change) override;

    mojo::Receiver<::network::mojom::CookieChangeListener>
        cookie_listener_receiver_{this};

    struct CookieModified {
      // Indicates whether or not cookie on the bfcache entry has been modified
      // while the entry is in bfcache.
      bool cookie_modified = false;
      // Indicates whether or not HTTPOnly cookie on the bfcache entry
      // has been modified while the entry is in bfcache.
      bool http_only_cookie_modified = false;
    };
    // Only populated when |AllowStoringPagesWithCacheControlNoStore()| is true.
    absl::optional<CookieModified> cookie_modified_;

    std::unique_ptr<StoredPage> stored_page_;
  };

  // UnloadSupportStrategy is possible actions to take against pages with
  // "unload" handlers.
  // TODO(crbug.com/1201653): Consider making this private.
  enum class UnloadSupportStrategy {
    kAlways,
    kOptInHeaderRequired,
    kNo,
  };

  BackForwardCacheImpl();

  BackForwardCacheImpl(const BackForwardCacheImpl&) = delete;
  BackForwardCacheImpl& operator=(const BackForwardCacheImpl&) = delete;

  ~BackForwardCacheImpl() override;

  // Returns whether MediaSession's playback state change is allowed for the
  // BackForwardCache.
  static bool IsMediaSessionPlaybackStateChangedAllowed();

  // Returns whether MediaSession's service is allowed for the BackForwardCache.
  static bool IsMediaSessionServiceAllowed();

  // Returns whether a RenderFrameHost can be stored into the BackForwardCache
  // right now. Depends on the |render_frame_host| and its children's state.
  // Should only be called after we've navigated away from |render_frame_host|,
  // which means nothing about the page can change (usage of blocklisted
  // features, pending navigations, load state, etc.) anymore.
  // Note that criteria for storing and restoring can be different.
  BackForwardCacheCanStoreDocumentResult CanStorePageNow(
      RenderFrameHostImpl* render_frame_host);

  // Whether a RenderFrameHost could be stored into the BackForwardCache at some
  // point in the future. Different than CanStorePageNow() above, we won't check
  // for properties of |render_frame_host| that might change in the future such
  // as usage of certain APIs, loading state, existence of pending navigation
  // requests, etc. This should be treated as a "best guess" on whether a page
  // still has a chance to be stored in the back-forward cache later on, and
  // should not be used as a final check before storing a page to the
  // back-forward cache (for that, use CanStorePageNow() instead).
  BackForwardCacheCanStoreDocumentResult CanPotentiallyStorePageLater(
      RenderFrameHostImpl* render_frame_host);

  // Moves the specified BackForwardCache entry into the BackForwardCache. It
  // can be reused in a future history navigation by using RestoreEntry(). When
  // the BackForwardCache is full, the least recently used document is evicted.
  // Precondition: CanStoreDocument(*(entry->render_frame_host)).
  void StoreEntry(std::unique_ptr<Entry> entry);

  // Ensures that the cache is within its size limits. This should be called
  // whenever events occur that could put the cache outside its limits. What
  // those events are depends on the cache limit policy.
  void EnforceCacheSizeLimit();

  // Returns a pointer to a cached BackForwardCache entry matching
  // |navigation_entry_id| if it exists in the BackForwardCache. Returns nullptr
  // if no matching entry is found.
  //
  // Note: The returned pointer should be used temporarily only within the
  // execution of a single task on the event loop. Beyond that, there is no
  // guarantee the pointer will be valid, because the document may be
  // removed/evicted from the cache.
  Entry* GetEntry(int navigation_entry_id);

  // During a history navigation, moves an entry out of the BackForwardCache
  // knowing its |navigation_entry_id|. |page_restore_params| includes
  // information that is needed by the entry's page after getting restored,
  // which includes the latest history information (offset, length) and the
  // timestamp corresponding to the start of the back-forward cached navigation,
  // which would be communicated to the page to allow it to record the latency
  // of this navigation.
  std::unique_ptr<Entry> RestoreEntry(
      int navigation_entry_id,
      blink::mojom::PageRestoreParamsPtr page_restore_params);

  // Evict all cached pages in the same BrowsingInstance as
  // |site_instance|.
  void EvictFramesInRelatedSiteInstances(SiteInstance* site_instance);

  // Immediately deletes all frames in the cache. This should only be called
  // when WebContents is being destroyed.
  void Shutdown();

  // Posts a task to destroy all frames in the BackForwardCache that have been
  // marked as evicted.
  void PostTaskToDestroyEvictedFrames();

  // Storing frames in back-forward cache is not supported indefinitely
  // due to potential privacy issues and memory leaks. Instead we are evicting
  // the frame from the cache after the time to live, which can be controlled
  // via experiment.
  static base::TimeDelta GetTimeToLiveInBackForwardCache();

  // Gets the maximum number of entries the BackForwardCache can hold per tab.
  static size_t GetCacheSize();

  // The back-forward cache is experimented on a limited set of URLs. This
  // method returns true if the |url| matches one of those. URL not matching
  // this won't enter the back-forward cache. This can still return true even
  // when BackForwardCache is disabled for metrics purposes. It checks
  // |IsHostPathAllowed| then |IsHostPathAllowed|
  bool IsAllowed(const GURL& current_url);
  // Returns true if the host and path are allowed according to the
  // "allowed_websites" and "blocked_webites" parameters of
  // |feature::kBackForwardCache|. An empty "allowed_websites" implies that all
  // websites are allowed.
  bool IsHostPathAllowed(const GURL& current_url);
  // Returns true if query does not contain any of the parameters in
  // "blocked_cgi_params" parameter of |feature::kBackForwardCache|. The
  // comparison is done by splitting the query string on "&" and looking for
  // exact matches in the list (parameter name and value). It does not consider
  // URL escaping.
  bool IsQueryAllowed(const GURL& current_url);

  // Called just before commit for a navigation that's served out of the back
  // forward cache. This method will disable eviction in renderers and invoke
  // |done_callback| when they are ready for the navigation to be committed.
  void WillCommitNavigationToCachedEntry(Entry& bfcache_entry,
                                         base::OnceClosure done_callback);

  // Returns the task runner that should be used by the eviction timer.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_for_testing_ ? task_runner_for_testing_
                                    : base::ThreadTaskRunnerHandle::Get();
  }

  // Inject task runner for precise timing control in browser tests.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    task_runner_for_testing_ = task_runner;
  }

  const std::list<std::unique_ptr<Entry>>& GetEntries();

  // BackForwardCache overrides:
  void Flush() override;
  void DisableForTesting(DisableForTestingReason reason) override;

  // RenderProcessHostInternalObserver methods
  void RenderProcessBackgroundedChanged(RenderProcessHostImpl* host) override;

  // Returns true if we are managing the cache size using foreground and
  // background limits (if finch parameter "foreground_cache_size" > 0).
  static bool UsingForegroundBackgroundCacheSizeLimit();

  // Used only for testing. This will include cache-control:no-store reasons if
  // there are any.
  BackForwardCacheCanStoreDocumentResult CanRestorePageNowForTesting(
      RenderFrameHostImpl* render_frame_host);

  // Returns true if one of the BFCache entries has a matching
  // BrowsingInstanceId/SiteInstanceId/RenderFrameProxyHost.
  // TODO(https://crbug.com/1243541): Remove these once the bug is fixed.
  bool IsBrowsingInstanceInBackForwardCacheForDebugging(
      BrowsingInstanceId browsing_instance_id);
  bool IsSiteInstanceInBackForwardCacheForDebugging(
      SiteInstanceId site_instance_id);
  bool IsProxyInBackForwardCacheForDebugging(RenderFrameProxyHost* proxy);

 private:
  // Destroys all evicted frames in the BackForwardCache.
  void DestroyEvictedFrames();

  // Helper for recursively checking each child's usage of blocklisted features.
  // See CanStorePageNow() and CanPotentiallyStorePageLater().
  void CheckDynamicBlocklistedFeaturesOnSubtree(
      BackForwardCacheCanStoreDocumentResult* result,
      RenderFrameHostImpl* render_frame_host);

  void CanStoreRenderFrameHostLater(
      BackForwardCacheCanStoreDocumentResult* result,
      RenderFrameHostImpl* render_frame_host);

  // Update the result to include CacheControlNoStore reasons if the flag is on.
  void UpdateCanStoreToIncludeCacheControlNoStore(
      BackForwardCacheCanStoreDocumentResult* result,
      RenderFrameHostImpl* render_frame_host);

  // Return the matching entry which has |page|.
  BackForwardCacheImpl::Entry* FindMatchingEntry(PageImpl& page);

  // If non-zero, the cache may contain at most this many entries with involving
  // foregrounded processes and the remaining space can only be used by entries
  // with no foregrounded processes. We can be less strict on memory usage of
  // background processes because Android will kill the process if memory
  // becomes scarce.
  static size_t GetForegroundedEntriesCacheSize();

  // Enforces a limit on the number of entries. Which entries are counted
  // towards the limit depends on the values of |foregrounded_only|. If it's
  // true it only considers entries that are associated with a foregrounded
  // process. Otherwise all entries are considered.
  size_t EnforceCacheSizeLimitInternal(size_t limit, bool foregrounded_only);

  // Updates |process_to_entry_map_| with processes from |entry|. These must
  // be called after adding or removing an entry in |entries_|.
  void AddProcessesForEntry(Entry& entry);
  void RemoveProcessesForEntry(Entry& entry);

  // Returns true if the flag is on for pages with cache-control:no-store to
  // get restored from back/forward cache unless cookies change.
  static bool AllowStoringPagesWithCacheControlNoStore();

  // Contains the set of stored Entries.
  // Invariant:
  // - Ordered from the most recently used to the last recently used.
  // - Once the list is full, the least recently used document is evicted.
  std::list<std::unique_ptr<Entry>> entries_;

  // Keeps track of the observed RenderProcessHosts. This is populated
  // from and kept in sync with |entries_|. The RenderProcessHosts are collected
  // from each Entry's RenderViewHosts. Every RenderProcessHost in here is
  // observed by |this|. Every RenderProcessHost in this is referenced by a
  // RenderViewHost in the Entry and so will be valid.
  std::multiset<RenderProcessHost*> observed_processes_;

  // Only used in tests. Whether the BackforwardCached has been disabled for
  // testing.
  bool is_disabled_for_testing_ = false;

  // Only used for tests. This task runner is used for precise injection in
  // browser tests and for timing control.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing_;

  // To enter the back-forward cache, the main document URL's must match one of
  // the field trial parameter "allowed_websites". This is represented here by a
  // set of host and path prefix. When |allowed_urls_| is empty, it means there
  // are no restrictions on URLs.
  const std::map<std::string,              // URL's host,
                 std::vector<std::string>  // URL's path prefix
                 >
      allowed_urls_;

  // This is an emergency kill switch per url to stop BFCache. The data will be
  // provided via the field trial parameter "blocked_websites".
  // "blocked_websites" have priority over "allowed_websites". This is
  // represented here by a set of host and path prefix.
  const std::map<std::string,              // URL's host,
                 std::vector<std::string>  // URL's path prefix
                 >
      blocked_urls_;

  // Data provided from the "blocked_cgi_params" feature param. If any of these
  // occur in the query of the URL then the page is not eligible for caching.
  // See |IsQueryAllowed|.
  const std::unordered_set<std::string> blocked_cgi_params_;

  const UnloadSupportStrategy unload_strategy_;

  base::WeakPtrFactory<BackForwardCacheImpl> weak_factory_;
};

// Allow external code to be notified when back-forward cache is disabled for a
// RenderFrameHost. This should be used only by the testing infrastructure which
// want to know the exact reason why the cache was disabled. There can be only
// one observer.
class CONTENT_EXPORT BackForwardCacheTestDelegate {
 public:
  BackForwardCacheTestDelegate();
  virtual ~BackForwardCacheTestDelegate();

  virtual void OnDisabledForFrameWithReason(
      GlobalRenderFrameHostId id,
      BackForwardCache::DisabledReason reason) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_
