// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/navigation_controller_delegate.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/content_export.h"
#include "content/common/navigation_client.mojom-forward.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/reload_type.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/network/public/mojom/source_location.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"

namespace blink {
struct NavigationDownloadPolicy;
}  // namespace blink

namespace content {
class FrameTree;
class FrameTreeNode;
class NavigationRequest;
class RenderFrameHostImpl;
class SiteInfo;
class SiteInstance;
struct LoadCommittedDetails;

// NavigationControllerImpl is 1:1 with FrameTree. See comments on the base
// class.
class CONTENT_EXPORT NavigationControllerImpl : public NavigationController {
 public:
  // This tracks one NavigationRequest navigating to a pending NavigationEntry.
  // In some cases, several NavigationRequests are referencing the same pending
  // NavigationEntry. For instance:
  // - A reload requested while a reload is already in progress.
  // - An history navigation causing several subframes to navigate.
  //
  // When no NavigationRequests are referencing the pending NavigationEntry
  // anymore, it should be discarded to avoid a URL spoof.
  //
  // The deletion is not always immediate, because it is not possible to delete
  // the entry while requesting a navigation to it at the same time. In this
  // case, the deletion happens later, when returning from the function.
  //
  // If the pending NavigationEntry is discarded before the PendingEntryRef(s),
  // then removing the last associated PendingEntryRef is a no-op. It is a no-op
  // forever, even if the entry becomes the pending NavigationEntry again in the
  // meantime. Rather than track the NavigationRequest or pending entry
  // explicitly, this ref class simply goes into a set that gets cleared with
  // each change to the pending entry
  class PendingEntryRef {
   public:
    explicit PendingEntryRef(
        base::WeakPtr<NavigationControllerImpl> controller);

    PendingEntryRef(const PendingEntryRef&) = delete;
    PendingEntryRef& operator=(const PendingEntryRef&) = delete;

    ~PendingEntryRef();

   private:
    base::WeakPtr<NavigationControllerImpl> controller_;
  };

  NavigationControllerImpl(BrowserContext* browser_context,
                           FrameTree& frame_tree,
                           NavigationControllerDelegate* delegate);

  NavigationControllerImpl(const NavigationControllerImpl&) = delete;
  NavigationControllerImpl& operator=(const NavigationControllerImpl&) = delete;

  ~NavigationControllerImpl() override;

  // NavigationController implementation:
  WebContents* DeprecatedGetWebContents() override;
  BrowserContext* GetBrowserContext() override;
  void Restore(int selected_navigation,
               RestoreType type,
               std::vector<std::unique_ptr<NavigationEntry>>* entries) override;
  NavigationEntryImpl* GetActiveEntry() override;
  NavigationEntryImpl* GetVisibleEntry() override;
  int GetCurrentEntryIndex() override;
  NavigationEntryImpl* GetLastCommittedEntry() override;
  int GetLastCommittedEntryIndex() override;
  bool CanViewSource() override;
  int GetEntryCount() override;
  NavigationEntryImpl* GetEntryAtIndex(int index) override;
  NavigationEntryImpl* GetEntryAtOffset(int offset) override;
  void DiscardNonCommittedEntries() override;
  NavigationEntryImpl* GetPendingEntry() override;
  int GetPendingEntryIndex() override;
  base::WeakPtr<NavigationHandle> LoadURL(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition type,
      const std::string& extra_headers) override;
  base::WeakPtr<NavigationHandle> LoadURLWithParams(
      const LoadURLParams& params) override;
  void LoadIfNecessary() override;
  base::WeakPtr<NavigationHandle> LoadPostCommitErrorPage(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      const std::string& error_page_html,
      net::Error error) override;
  bool CanGoBack() override;
  bool CanGoForward() override;
  bool CanGoToOffset(int offset) override;
  void GoBack() override;
  void GoForward() override;
  void GoToIndex(int index) override;
  void GoToOffset(int offset) override;
  bool RemoveEntryAtIndex(int index) override;
  void PruneForwardEntries() override;
  const SessionStorageNamespaceMap& GetSessionStorageNamespaceMap() override;
  SessionStorageNamespace* GetDefaultSessionStorageNamespace() override;
  bool NeedsReload() override;
  void SetNeedsReload() override;
  void CancelPendingReload() override;
  void ContinuePendingReload() override;
  bool IsInitialNavigation() override;
  bool IsInitialBlankNavigation() override;
  void Reload(ReloadType reload_type, bool check_for_repost) override;
  void NotifyEntryChanged(NavigationEntry* entry) override;
  void CopyStateFrom(NavigationController* source, bool needs_reload) override;
  void CopyStateFromAndPrune(NavigationController* source,
                             bool replace_entry) override;
  bool CanPruneAllButLastCommitted() override;
  void PruneAllButLastCommitted() override;
  void DeleteNavigationEntries(
      const DeletionPredicate& deletionPredicate) override;
  bool IsEntryMarkedToBeSkipped(int index) override;
  BackForwardCacheImpl& GetBackForwardCache() override;

  // Discards the pending entry if any. If this is caused by a navigation
  // committing a new entry, `commit_details` will contain the committed
  // navigation's details.
  void DiscardNonCommittedEntriesWithCommitDetails(
      LoadCommittedDetails* commit_details);

  // Creates the initial NavigationEntry for the NavigationController when its
  // FrameTree is being initialized. See NavigationEntry::IsInitialEntry() on
  // what this means.
  void CreateInitialEntry();

  // Starts a navigation in a newly created subframe as part of a history
  // navigation. Returns true if the history navigation could start, false
  // otherwise.  If this returns false, the caller should do a regular
  // navigation to the default src URL for the frame instead.
  bool StartHistoryNavigationInNewSubframe(
      RenderFrameHostImpl* render_frame_host,
      mojo::PendingAssociatedRemote<mojom::NavigationClient>*
          navigation_client);

  // Reloads the |frame_tree_node| and returns true. In some rare cases, there
  // is no history related to the frame, nothing happens and this returns false.
  bool ReloadFrame(FrameTreeNode* frame_tree_node);

  // Navigates to a specified offset from the "current entry". Currently records
  // a histogram indicating whether the session history navigation would only
  // affect frames within the subtree of |sandbox_frame_tree_node_id|, which
  // initiated the navigation. Navigating via this function is considered
  // renderer-initiated, since it is only invoked when the renderer requests a
  // history traversal.
  void GoToOffsetInSandboxedFrame(int offset, int sandbox_frame_tree_node_id);

  // Navigates to the specified offset from the "current entry" and marks the
  // navigations as initiated by the renderer.
  void GoToOffsetFromRenderer(int offset);

#if defined(OS_ANDROID)
  // The difference between (Can)GoToOffsetWithSkipping and
  // (Can)GoToOffset/(Can)GoToOffsetInSandboxedFrame is that this respects the
  // history manipulation intervention and will exclude skippable entries.
  // These should only be used for browser-initiated navigaitons.
  bool CanGoToOffsetWithSkipping(int offset);
  void GoToOffsetWithSkipping(int offset);
#endif

  // Called when a document requests a navigation through a
  // RenderFrameProxyHost.
  void NavigateFromFrameProxy(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const blink::LocalFrameToken* initiator_frame_token,
      int initiator_process_id,
      const absl::optional<url::Origin>& initiator_origin,
      bool is_renderer_initiated,
      SiteInstance* source_site_instance,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry,
      blink::NavigationDownloadPolicy download_policy,
      const std::string& method,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const std::string& extra_headers,
      network::mojom::SourceLocationPtr source_location,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      const absl::optional<blink::Impression>& impression);

  // Navigates to the history entry associated with the given app history |key|.
  // Searches |entries_| for a FrameNavigationEntry associated with |node|
  // that has |key| as its app history key. Searches back from the current
  // index, then forward, so if there are multiple entries with the same key,
  // the nearest to current should be selected. Stops searching in the current
  // direction if it finds a NavigationEntry without a FrameNavigationEntry for
  // |node|, or if the FrameNavigationEntry doesn't match origin or site
  // instance.
  //
  // If no matching entry is found, the navigation is dropped. The renderer
  // should only send the navigation to the browser if it believes the entry is
  // in |entries_|, but it might be wrong (if the entry was dropped from
  // |entries_|, or due to a race condition) or compromised.
  // If a matching entry is found, navigate to that entry and proceed like any
  // other history navigation.
  void NavigateToAppHistoryKey(FrameTreeNode* node, const std::string& key);

  // Whether this is the initial navigation in an unmodified new tab.  In this
  // case, we know there is no content displayed in the page.
  bool IsUnmodifiedBlankTab();

  // The session storage namespace that all child RenderViews associated with
  // |site_info| should use.
  SessionStorageNamespace* GetSessionStorageNamespace(
      const SiteInfo& site_info);

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationController.
  int GetIndexOfEntry(const NavigationEntryImpl* entry) const;

  // Return the index of the entry with the given unique id, or -1 if not found.
  int GetEntryIndexWithUniqueID(int nav_entry_id) const;

  // Return the entry with the given unique id, or null if not found.
  NavigationEntryImpl* GetEntryWithUniqueID(int nav_entry_id) const;
  // Same as above method, but also includes the pending entry in the search
  // space.
  NavigationEntryImpl* GetEntryWithUniqueIDIncludingPending(
      int nav_entry_id) const;

  NavigationControllerDelegate* delegate() const { return delegate_; }

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NeedsReloadType {
    kRequestedByClient = 0,
    kRestoreSession = 1,
    kCopyStateFrom = 2,
    kCrashedSubframe = 3,
    kMaxValue = kCrashedSubframe
  };

  // Request a reload to happen when activated.  Same as the public
  // SetNeedsReload(), but takes in a |type| which specifies why the reload is
  // being requested.
  void SetNeedsReload(NeedsReloadType type);

  // For use by WebContentsImpl ------------------------------------------------

  // Visit all FrameNavigationEntries as well as all frame trees and register
  // any instances of |origin| as non-isolated with their respective
  // BrowsingInstances. This is important when |origin| requests isolation, so
  // that we only do so in BrowsingInstances that haven't seen it before.
  void RegisterExistingOriginToPreventOptInIsolation(const url::Origin& origin);

  // Allow renderer-initiated navigations to create a pending entry when the
  // provisional load starts.
  void SetPendingEntry(std::unique_ptr<NavigationEntryImpl> entry);

  // Handles updating the navigation state after the renderer has navigated.
  // This is used by the WebContentsImpl.
  //
  // If a new entry is created, it will return true and will have filled the
  // given details structure and broadcast the NOTIFY_NAV_ENTRY_COMMITTED
  // notification. The caller can then use the details without worrying about
  // listening for the notification.
  //
  // In the case that nothing has changed, the details structure is undefined
  // and it will return false.
  //
  // |was_on_initial_empty_document| indicates whether the document being
  // navigated away from was an initial empty document.
  //
  // |previous_document_was_activated| is true if the previous document had user
  // interaction. This is used for a new renderer-initiated navigation to decide
  // if the page that initiated the navigation should be skipped on
  // back/forward button.
  bool RendererDidNavigate(RenderFrameHostImpl* rfh,
                           const mojom::DidCommitProvisionalLoadParams& params,
                           LoadCommittedDetails* details,
                           bool is_same_document_navigation,
                           bool was_on_initial_empty_document,
                           bool previous_document_was_activated,
                           NavigationRequest* navigation_request);

  // Notifies us that we just became active. This is used by the WebContentsImpl
  // so that we know to load URLs that were pending as "lazy" loads.
  void SetActive(bool is_active);

  // Sets the SessionStorageNamespace for the given |partition_id|. This is
  // used during initialization of a new NavigationController to allow
  // pre-population of the SessionStorageNamespace objects. Session restore,
  // prerendering, and the implementation of window.open() are the primary users
  // of this API.
  //
  // Calling this function when a SessionStorageNamespace has already been
  // associated with a |partition_id| will CHECK() fail.
  void SetSessionStorageNamespace(
      const StoragePartitionId& partition_id,
      SessionStorageNamespace* session_storage_namespace);

  // Random data ---------------------------------------------------------------

  FrameTree& frame_tree() { return frame_tree_; }

  SSLManager* ssl_manager() { return &ssl_manager_; }

  // Maximum number of entries before we start removing entries from the front.
  static void set_max_entry_count_for_testing(size_t max_entry_count) {
    max_entry_count_for_testing_ = max_entry_count;
  }
  static size_t max_entry_count();

  void SetGetTimestampCallbackForTest(
      const base::RepeatingCallback<base::Time()>& get_timestamp_callback);

  // Discards only the pending entry. |was_failure| should be set if the pending
  // entry is being discarded because it failed to load.
  void DiscardPendingEntry(bool was_failure);

  // Sets a flag on the pending NavigationEntryImpl instance if any that the
  // navigation failed due to an SSL error.
  void SetPendingNavigationSSLError(bool error);

// Returns true if the string corresponds to a valid data URL, false
// otherwise.
#if defined(OS_ANDROID)
  static bool ValidateDataURLAsString(
      const scoped_refptr<const base::RefCountedString>& data_url_as_string);
#endif

  // Invoked when a user activation occurs within the page, so that relevant
  // entries can be updated as needed.
  void NotifyUserActivation();

  // Tracks a new association between the current pending entry and a
  // NavigationRequest. Callers are responsible for only calling this for
  // requests corresponding to the current pending entry.
  std::unique_ptr<PendingEntryRef> ReferencePendingEntry();

  // Another page accessed the initial empty main document, which means it
  // is no longer safe to display a pending URL without risking a URL spoof.
  void DidAccessInitialMainDocument();

  // The state for the page changed and should be updated in session history.
  void UpdateStateForFrame(RenderFrameHostImpl* render_frame_host,
                           const blink::PageState& page_state);

  // Like NavigationController::CreateNavigationEntry, but takes an extra
  // argument, |source_site_instance|.
  static std::unique_ptr<NavigationEntryImpl> CreateNavigationEntry(
      const GURL& url,
      Referrer referrer,
      absl::optional<url::Origin> initiator_origin,
      SiteInstance* source_site_instance,
      ui::PageTransition transition,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      BrowserContext* browser_context,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);

  // Called just before sending the commit to the renderer. Walks the
  // session history entries for the committing FrameTreeNode, forward and
  // backward from the pending entry. All contiguous and same-origin
  // FrameNavigationEntries are serialized and added to |request|'s commit
  // params.
  void PopulateAppHistoryEntryVectors(NavigationRequest* request);

 private:
  friend class RestoreHelper;

  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, Basic);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, SingleDuplicate);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, ManyDuplicates);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, ClockBackwardsJump);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerBrowserTest, PostThenReload);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerBrowserTest,
                           PostThenReplaceStateThenReload);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerBrowserTest,
                           PostThenPushStateThenReloadThenHistory);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerBrowserTest,
                           PostThenFragmentNavigationThenReloadThenHistory);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationControllerBrowserTest,
      PostThenBrowserInitiatedFragmentNavigationThenReload);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerBrowserTest, PostSubframe);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerDisableHistoryIntervention,
                           GoToOffsetWithSkippingDisableHistoryIntervention);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           GoToOffsetWithSkippingEnableHistoryIntervention);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           SetSkipOnBackForwardDoSkipForGoToOffsetWithSkipping);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerHistoryInterventionBrowserTest,
                           SetSkipOnBackForwardDoNotSkipForGoToOffset);

  // Defines possible actions that are returned by
  // DetermineActionForHistoryNavigation().
  enum class HistoryNavigationAction {
    kStopLooking,
    kKeepLooking,
    kSameDocument,
    kDifferentDocument,
  };

  // Helper class to smooth out runs of duplicate timestamps while still
  // allowing time to jump backwards.
  class CONTENT_EXPORT TimeSmoother {
   public:
    // Returns |t| with possibly some time added on.
    base::Time GetSmoothedTime(base::Time t);

   private:
    // |low_water_mark_| is the first time in a sequence of adjusted
    // times and |high_water_mark_| is the last.
    base::Time low_water_mark_;
    base::Time high_water_mark_;
  };

  // The repost dialog is suppressed during testing. However, it should be shown
  // in some tests. This allows a test to elect to allow the repost dialog to
  // show for a scoped duration.
  class CONTENT_EXPORT ScopedShowRepostDialogForTesting {
   public:
    ScopedShowRepostDialogForTesting();
    ~ScopedShowRepostDialogForTesting();

    ScopedShowRepostDialogForTesting(const ScopedShowRepostDialogForTesting&) =
        delete;
    ScopedShowRepostDialogForTesting& operator=(
        const ScopedShowRepostDialogForTesting&) = delete;

   private:
    const bool was_disallowed_;
  };

  // Navigates in session history to the given index. If
  // |sandbox_frame_tree_node_id| is valid, then this request came
  // from a sandboxed iframe with top level navigation disallowed. This
  // is currently only used for tracking metrics.
  void GoToIndex(int index,
                 int sandbox_frame_tree_node_id,
                 bool is_browser_initiated);

  // Starts a navigation to an already existing pending NavigationEntry.
  // Currently records a histogram indicating whether the session history
  // navigation would only affect frames within the subtree of
  // |sandbox_frame_tree_node_id|, which initiated the navigation.
  void NavigateToExistingPendingEntry(ReloadType reload_type,
                                      int sandboxed_source_frame_tree_node_id,
                                      bool is_browser_initiated);

  // Helper function used by FindFramesToNavigate to determine the appropriate
  // action to take for a particular frame while navigating to
  // |pending_entry_|.
  HistoryNavigationAction DetermineActionForHistoryNavigation(
      FrameTreeNode* frame,
      ReloadType reload_type);

  // Recursively identifies which frames need to be navigated for a navigation
  // to |pending_entry_|, starting at |frame| and exploring its children.
  // |same_document_loads| and |different_document_loads| will be filled with
  // the NavigationRequests needed to navigate to |pending_entry_|.
  void FindFramesToNavigate(
      FrameTreeNode* frame,
      ReloadType reload_type,
      bool is_browser_initiated,
      std::vector<std::unique_ptr<NavigationRequest>>* same_document_loads,
      std::vector<std::unique_ptr<NavigationRequest>>*
          different_document_loads);

  // Starts a new navigation based on |load_params|, that doesn't correspond to
  // an existing NavigationEntry.
  base::WeakPtr<NavigationHandle> NavigateWithoutEntry(
      const LoadURLParams& load_params);

  // Handles a navigation to a renderer-debug URL.
  void HandleRendererDebugURL(FrameTreeNode* frame_tree_node, const GURL& url);

  // Creates and returns a NavigationEntry based on |load_params| for a
  // navigation in |node|.
  // |override_user_agent|, |should_replace_current_entry| and
  // |has_user_gesture| will override the values from |load_params|. The same
  // values should be passed to CreateNavigationRequestFromLoadParams.
  std::unique_ptr<NavigationEntryImpl> CreateNavigationEntryFromLoadParams(
      FrameTreeNode* node,
      const LoadURLParams& load_params,
      bool override_user_agent,
      bool should_replace_current_entry,
      bool has_user_gesture);

  // Creates and returns a NavigationRequest based on |load_params| for a
  // new navigation in |node|.
  // Will return nullptr if the parameters are invalid and the navigation cannot
  // start.
  // |override_user_agent|, |should_replace_current_entry| and
  // |has_user_gesture| will override the values from |load_params|. The same
  // values should be passed to CreateNavigationEntryFromLoadParams.
  // TODO(clamy): Remove the dependency on NavigationEntry and
  // FrameNavigationEntry.
  std::unique_ptr<NavigationRequest> CreateNavigationRequestFromLoadParams(
      FrameTreeNode* node,
      const LoadURLParams& load_params,
      bool override_user_agent,
      bool should_replace_current_entry,
      bool has_user_gesture,
      network::mojom::SourceLocationPtr source_location,
      ReloadType reload_type,
      NavigationEntryImpl* entry,
      FrameNavigationEntry* frame_entry);

  // Creates and returns a NavigationRequest for a navigation to |entry|. Will
  // return nullptr if the parameters are invalid and the navigation cannot
  // start.
  // TODO(clamy): Ensure this is only called for navigations to existing
  // NavigationEntries.
  std::unique_ptr<NavigationRequest> CreateNavigationRequestFromEntry(
      FrameTreeNode* frame_tree_node,
      NavigationEntryImpl* entry,
      FrameNavigationEntry* frame_entry,
      ReloadType reload_type,
      bool is_same_document_history_load,
      bool is_history_navigation_in_new_child_frame,
      bool is_browser_initiated);

  // Returns whether there is a pending NavigationEntry whose unique ID matches
  // the given NavigationRequest's pending_nav_entry_id.
  bool PendingEntryMatchesRequest(NavigationRequest* request) const;

  // Classifies the given renderer navigation (see the NavigationType enum).
  NavigationType ClassifyNavigation(
      RenderFrameHostImpl* rfh,
      const mojom::DidCommitProvisionalLoadParams& params,
      NavigationRequest* navigation_request,
      LoadCommittedDetails* load_committed_details);

  // Handlers for the different types of navigation types. They will actually
  // handle the navigations corresponding to the different NavClasses above.
  // They will NOT broadcast the commit notification, that should be handled by
  // the caller.
  //
  // RendererDidNavigateAutoSubframe is special, it may not actually change
  // anything if some random subframe is loaded. It will return true if anything
  // changed, or false if not.
  //
  // The NewEntry and NewSubframe functions take in |replace_entry| to pass to
  // InsertOrReplaceEntry, in case the newly created NavigationEntry is meant to
  // replace the current one (e.g., for location.replace or successful loads
  // after net errors), in contrast to updating a NavigationEntry in place
  // (e.g., for history.replaceState).
  void RendererDidNavigateToNewEntry(
      RenderFrameHostImpl* rfh,
      const mojom::DidCommitProvisionalLoadParams& params,
      bool is_same_document,
      bool replace_entry,
      bool previous_document_was_activated,
      NavigationRequest* request,
      LoadCommittedDetails* details);
  void RendererDidNavigateToExistingEntry(
      RenderFrameHostImpl* rfh,
      const mojom::DidCommitProvisionalLoadParams& params,
      bool is_same_document,
      bool was_restored,
      NavigationRequest* request,
      bool keep_pending_entry,
      LoadCommittedDetails* details);
  void RendererDidNavigateNewSubframe(
      RenderFrameHostImpl* rfh,
      const mojom::DidCommitProvisionalLoadParams& params,
      bool is_same_document,
      bool replace_entry,
      bool previous_document_was_activated,
      NavigationRequest* request,
      LoadCommittedDetails* details);
  bool RendererDidNavigateAutoSubframe(
      RenderFrameHostImpl* rfh,
      const mojom::DidCommitProvisionalLoadParams& params,
      bool is_same_document,
      bool was_on_initial_empty_document,
      NavigationRequest* request,
      LoadCommittedDetails* details);

  // Allows the derived class to issue notifications that a load has been
  // committed. This will fill in the active entry to the details structure.
  void NotifyNavigationEntryCommitted(LoadCommittedDetails* details);

  // Updates the virtual URL of an entry to match a new URL, for cases where
  // the real renderer URL is derived from the virtual URL, like view-source:
  void UpdateVirtualURLToURL(NavigationEntryImpl* entry, const GURL& new_url);

  // Invoked after session/tab restore or cloning a tab. Resets the transition
  // type of the entries, updates the max page id and creates the active
  // contents.
  void FinishRestore(int selected_index, RestoreType type);

  // Inserts a new entry or replaces the current entry with a new one, removing
  // all entries after it. The new entry will become the active one.
  // If |was_post_commit_error_| is set, the last committed entry will be saved,
  // the new entry will replace it, and on any navigation away from the new
  // entry or on reloads, the old one will replace |entry|.
  void InsertOrReplaceEntry(std::unique_ptr<NavigationEntryImpl> entry,
                            bool replace,
                            bool was_post_commit_error,
                            bool is_in_fenced_frame_tree,
                            LoadCommittedDetails* details);

  // Removes the entry at |index|, as long as it is not the current entry.
  void RemoveEntryAtIndexInternal(int index);

  // If we have the maximum number of entries, remove the oldest entry that is
  // marked to be skipped on back/forward button, in preparation to add another.
  // If no entry is skippable, then the oldest entry will be pruned.
  void PruneOldestSkippableEntryIfFull();

  // Removes all entries except the last committed entry.  If there is a new
  // pending navigation it is preserved. In contrast to
  // PruneAllButLastCommitted() this does not update the session history of the
  // RenderView.  Callers must ensure that |CanPruneAllButLastCommitted| returns
  // true before calling this.
  void PruneAllButLastCommittedInternal();

  // Inserts up to |max_index| entries from |source| into this. This does NOT
  // adjust any of the members that reference entries_
  // (last_committed_entry_index_ or pending_entry_index_)
  void InsertEntriesFrom(NavigationControllerImpl* source, int max_index);

  // Returns the navigation index that differs from the current entry by the
  // specified |offset|.  The index returned is not guaranteed to be valid.
  // This does not account for skippable entries or the history manipulation
  // intervention.
  int GetIndexForOffset(int offset);

  // History Manipulation intervention:
  // The previous document that started this navigation needs to be skipped in
  // subsequent back/forward UI navigations if it never received any user
  // gesture. This is to intervene against pages that manipulate the history
  // such that the user is not able to go back to the last site they interacted
  // with (crbug.com/907167).
  // Note that this function must be called before the new navigation entry is
  // inserted in |entries_| to make sure UKM reports the URL of the document
  // adding the entry.
  void SetShouldSkipOnBackForwardUIIfNeeded(
      bool replace_entry,
      bool previous_document_was_activated,
      bool is_renderer_initiated,
      ukm::SourceId previous_page_load_ukm_source_id);

  // This function sets all same document entries with the same value
  // of skippable flag. This is to avoid back button abuse by inserting
  // multiple history entries and also to help valid cases where a user gesture
  // on the document should apply to all same document history entries and none
  // should be skipped. All entries belonging to the same document as the entry
  // at |reference_index| will get their skippable flag set to |skippable|.
  void SetSkippableForSameDocumentEntries(int reference_index, bool skippable);

  // Called when one PendingEntryRef is deleted. When all of the refs for the
  // current pending entry have been deleted, this automatically discards the
  // pending NavigationEntry.
  void PendingEntryRefDeleted(PendingEntryRef* ref);

  // Computes the policy container policies to be stored in the
  // FrameNavigationEntry by RendererDidNavigate.
  std::unique_ptr<PolicyContainerPolicies>
  ComputePolicyContainerPoliciesForFrameEntry(RenderFrameHostImpl* rfh,
                                              bool is_same_document,
                                              const GURL& url);

  // Adds details from a committed navigation to `entry` and the
  // FrameNavigationEntry corresponding to `rfh`.
  void UpdateNavigationEntryDetails(
      NavigationEntryImpl* entry,
      RenderFrameHostImpl* rfh,
      const mojom::DidCommitProvisionalLoadParams& params,
      NavigationRequest* request,
      NavigationEntryImpl::UpdatePolicy update_policy,
      bool is_new_entry,
      LoadCommittedDetails* commit_details);

  // Broadcasts this controller's session history offset and length to all
  // renderers involved in rendering the current page. The offset is
  // GetLastCommittedEntryIndex() and length is GetEntryCount().
  void BroadcastHistoryOffsetAndLength();

  // Helper functions used to determine if it is safe to change the internal
  // representation of StoragePartitionId.
  //
  // Called when a new StoragePartitionId is added to
  // `session_storage_namespace_map_` and adds an entry to
  // `partition_config_to_id_map_`.
  void OnStoragePartitionIdAdded(const StoragePartitionId& partition_id);
  // Called to log a crash dump when unique string representations result in
  // the same StoragePartitionConfig, or an ID used to lookup a namespace
  // contains a different config than the one used when the namespace was
  // added to the map. Both situations imply that there is not a 1:1 mapping
  // between representations.
  void LogStoragePartitionIdCrashKeys(
      const StoragePartitionId& original_partition_id,
      const StoragePartitionId& new_partition_id);

  // Used by PopulateAppHistoryEntryVectors to initialize a single vector.
  enum class Direction { kForward, kBack };
  std::vector<blink::mojom::AppHistoryEntryPtr>
  PopulateSingleAppHistoryEntryVector(Direction direction,
                                      int entry_index,
                                      const url::Origin& pending_origin,
                                      FrameTreeNode* node,
                                      SiteInstance* site_instance,
                                      int64_t previous_item_sequence_number);
  // Helper for NavigateToAppHistoryKey(). Ensures that we only navigate to
  // |target_entry| if it matches |current_entry|'s origin and site instance, as
  // well as having |app_history_key| as its key.
  HistoryNavigationAction ShouldNavigateToEntryForAppHistoryKey(
      FrameNavigationEntry* current_entry,
      FrameNavigationEntry* target_entry,
      const std::string& app_history_key);

  // Whether to maintain a session history with just one entry.
  //
  // This returns true for a prerendering page and for fenced frames.
  // `frame_tree_node` is checked to see if it belongs to a frame tree for
  // prerendering or for a fenced frame.
  // Explainer:
  // https://github.com/jeremyroman/alternate-loading-modes/blob/main/browsing-context.md#session-history)
  //
  // Portals will be added to this in the future.
  bool ShouldMaintainTrivialSessionHistory(
      const FrameTreeNode* frame_tree_node) const;

  // ---------------------------------------------------------------------------

  // The FrameTree this instance belongs to. Each FrameTree gets its own
  // NavigationController.
  FrameTree& frame_tree_;

  // The user browser context associated with this controller.
  const raw_ptr<BrowserContext> browser_context_;

  // List of |NavigationEntry|s for this controller.
  std::vector<std::unique_ptr<NavigationEntryImpl>> entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  //
  // This may refer to an item in the entries_ list if the pending_entry_index_
  // != -1, or it may be its own entry that should be deleted. Be careful with
  // the memory management.
  raw_ptr<NavigationEntryImpl> pending_entry_ = nullptr;

  // This keeps track of the NavigationRequests associated with the pending
  // NavigationEntry. When all of them have been deleted, or have stopped
  // loading, the pending NavigationEntry can be discarded.
  //
  // This is meant to avoid a class of URL spoofs where the navigation is
  // canceled, but the stale pending NavigationEntry is left in place.
  std::set<PendingEntryRef*> pending_entry_refs_;

  // If a new entry fails loading, details about it are temporarily held here
  // until the error page is shown (or 0 otherwise).
  //
  // TODO(avi): We need a better way to handle the connection between failed
  // loads and the subsequent load of the error page. This current approach has
  // issues: 1. This might hang around longer than we'd like if there is no
  // error page loaded, and 2. This doesn't work very well for frames.
  // http://crbug.com/474261
  int failed_pending_entry_id_ = 0;

  // The index of the currently visible entry.
  int last_committed_entry_index_ = -1;

  // The index of the pending entry if it is in entries_, or -1 if
  // pending_entry_ is a new entry (created by LoadURL).
  int pending_entry_index_ = -1;

  // The delegate associated with the controller. Possibly null during
  // setup.
  raw_ptr<NavigationControllerDelegate> delegate_;

  // Manages the SSL security UI.
  SSLManager ssl_manager_;

  // Whether we need to be reloaded when made active.
  bool needs_reload_ = false;

  // Source of when |needs_reload_| is set. Only valid when |needs_reload_|
  // is set.
  NeedsReloadType needs_reload_type_ = NeedsReloadType::kRequestedByClient;

  // Whether this is the initial navigation.
  // Becomes false when initial navigation commits.
  bool is_initial_navigation_ = true;

  // Prevent unsafe re-entrant calls to NavigateToPendingEntry.
  bool in_navigate_to_pending_entry_ = false;

  // Used to find the appropriate SessionStorageNamespace for the storage
  // partition of a NavigationEntry.
  //
  // A NavigationController may contain NavigationEntries that correspond to
  // different StoragePartitions. Even though they are part of the same
  // NavigationController, only entries in the same StoragePartition may
  // share session storage state with one another.
  SessionStorageNamespaceMap session_storage_namespace_map_;

  // Temporary map that is being used to verify that there is a 1:1
  // relationship between the string representation used as the key in
  // `session_storage_namespace_map_` and the StoragePartitionConfig
  // representation that we plan to migrate the map key to.
  // TODO(acolwell): Remove this map once we have enough data to determine if
  // it is safe to change representations or not.
  std::map<StoragePartitionConfig, StoragePartitionId>
      partition_config_to_id_map_;

  // The maximum number of entries that a navigation controller can store.
  static size_t max_entry_count_for_testing_;

  // If a repost is pending, its type (RELOAD or RELOAD_BYPASSING_CACHE),
  // NO_RELOAD otherwise.
  ReloadType pending_reload_ = ReloadType::NONE;

  // Used to get timestamps for newly-created navigation entries.
  base::RepeatingCallback<base::Time()> get_timestamp_callback_;

  // Used to smooth out timestamps from |get_timestamp_callback_|.
  // Without this, whenever there is a run of redirects or
  // code-generated navigations, those navigations may occur within
  // the timer resolution, leading to things sometimes showing up in
  // the wrong order in the history view.
  TimeSmoother time_smoother_;

  // BackForwardCache:
  //
  // Stores frozen RenderFrameHost. Restores them on history navigation.
  // See BackForwardCache class documentation.
  BackForwardCacheImpl back_forward_cache_;

  // Holds the entry that was committed at the time an error page was triggered
  // due to a call to LoadPostCommitErrorPage. The error entry will take its
  // place until the user navigates again, at which point it will go back into
  // the entry list instead of the error entry. Set to nullptr if there is no
  // post commit error entry. Note that this entry must always correspond to the
  // last committed entry index, and that there can be only a single post-commit
  // error page entry in its place in entries_. This ensures that its spot in
  // entries_ cannot go away (e.g., due to PruneForwardEntries) and that it can
  // go back into place after any subsequent commit.
  std::unique_ptr<NavigationEntryImpl> entry_replaced_by_post_commit_error_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<NavigationControllerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_IMPL_H_
