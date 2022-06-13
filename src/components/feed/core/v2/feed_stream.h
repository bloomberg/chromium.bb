// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/version.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/persistent_key_value_store_impl.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/request_throttler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream/privacy_notice_card_tracker.h"
#include "components/feed/core/v2/stream/upload_criteria.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_surface_set.h"
#include "components/feed/core/v2/tasks/load_more_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/core/v2/wire_response_translator.h"
#include "components/offline_pages/task/task_queue.h"
#include "components/prefs/pref_member.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace feed {
namespace feed_stream {
class UnreadContentNotifier;
}
class NoticeCardTracker;
class FeedNetwork;
class FeedStore;
class WebFeedSubscriptionCoordinator;
class ImageFetcher;
class MetricsReporter;
class RefreshTaskScheduler;
class PersistentKeyValueStoreImpl;
class StreamModel;
class SurfaceUpdater;

// Implements FeedApi. |FeedStream| additionally exposes functionality
// needed by other classes within the Feed component.
class FeedStream : public FeedApi,
                   public offline_pages::TaskQueue::Delegate,
                   public MetricsReporter::Delegate,
                   public StreamModel::StoreObserver {
 public:
  class Delegate : public WebFeedSubscriptionCoordinator::Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if Chrome's EULA has been accepted.
    virtual bool IsEulaAccepted() = 0;
    // Returns true if the device is offline.
    virtual bool IsOffline() = 0;
    virtual DisplayMetrics GetDisplayMetrics() = 0;
    virtual std::string GetLanguageTag() = 0;
    virtual bool IsAutoplayEnabled() = 0;
    virtual void ClearAll() = 0;
    virtual std::string GetSyncSignedInGaia() = 0;
    virtual std::string GetSyncSignedInEmail() = 0;
    virtual void PrefetchImage(const GURL& url) = 0;
    virtual void RegisterExperiments(const Experiments& experiments) = 0;
  };

  FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
             MetricsReporter* metrics_reporter,
             Delegate* delegate,
             PrefService* profile_prefs,
             FeedNetwork* feed_network,
             ImageFetcher* image_fetcher,
             FeedStore* feed_store,
             PersistentKeyValueStoreImpl* persistent_key_value_store,
             const ChromeInfo& chrome_info);
  ~FeedStream() override;

  FeedStream(const FeedStream&) = delete;
  FeedStream& operator=(const FeedStream&) = delete;

  // FeedApi.

  WebFeedSubscriptionCoordinator& subscriptions() override;
  bool IsActivityLoggingEnabled(const StreamType& stream_type) const override;
  std::string GetSessionId() const override;
  void AttachSurface(FeedStreamSurface*) override;
  void DetachSurface(FeedStreamSurface*) override;
  void AddUnreadContentObserver(const StreamType& stream_type,
                                UnreadContentObserver* observer) override;
  void RemoveUnreadContentObserver(const StreamType& stream_type,
                                   UnreadContentObserver* observer) override;
  bool IsArticlesListVisible() override;
  std::string GetClientInstanceId() const override;
  void ExecuteRefreshTask(RefreshTaskId task_id) override;
  ImageFetchId FetchImage(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) override;
  void CancelImageFetch(ImageFetchId id) override;
  PersistentKeyValueStoreImpl& GetPersistentKeyValueStore() override;
  void LoadMore(const FeedStreamSurface& surface,
                base::OnceCallback<void(bool)> callback) override;
  void ManualRefresh(const StreamType& stream_type,
                     base::OnceCallback<void(bool)> callback) override;
  void ExecuteOperations(
      const StreamType& stream_type,
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChange(
      const StreamType& stream_type,
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChangeFromPackedData(
      const StreamType& stream_type,
      base::StringPiece data) override;
  bool CommitEphemeralChange(const StreamType& stream_type,
                             EphemeralChangeId id) override;
  bool RejectEphemeralChange(const StreamType& stream_type,
                             EphemeralChangeId id) override;
  void ProcessThereAndBackAgain(base::StringPiece data) override;
  void ProcessThereAndBackAgain(
      base::StringPiece data,
      const feedui::LoggingParameters& logging_parameters) override;
  void ProcessViewAction(base::StringPiece data) override;
  void ProcessViewAction(
      base::StringPiece data,
      const feedui::LoggingParameters& logging_parameters) override;
  bool WasUrlRecentlyNavigatedFromFeed(const GURL& url) override;
  DebugStreamData GetDebugStreamData() override;
  void ForceRefreshForDebugging(const StreamType& stream_type) override;
  std::string DumpStateForDebugging() override;
  void SetForcedStreamUpdateForDebugging(
      const feedui::StreamUpdate& stream_update) override;

  void ReportSliceViewed(SurfaceId surface_id,
                         const StreamType& stream_type,
                         const std::string& slice_id) override;
  void ReportFeedViewed(const StreamType& stream_type,
                        SurfaceId surface_id) override;
  void ReportPageLoaded() override;
  void ReportOpenAction(const GURL& url,
                        const StreamType& stream_type,
                        const std::string& slice_id) override;
  void ReportOpenVisitComplete(base::TimeDelta visit_time) override;
  void ReportOpenInNewTabAction(const GURL& url,
                                const StreamType& stream_type,
                                const std::string& slice_id) override;
  void ReportStreamScrolled(const StreamType& stream_type,
                            int distance_dp) override;
  void ReportStreamScrollStart() override;
  void ReportOtherUserAction(const StreamType& stream_type,
                             FeedUserActionType action_type) override;
  void ReportNoticeCreated(const StreamType& stream_type,
                           const std::string& key) override;
  void ReportNoticeViewed(const StreamType& stream_type,
                          const std::string& key) override;
  void ReportNoticeOpenAction(const StreamType& stream_type,
                              const std::string& key) override;
  void ReportNoticeDismissed(const StreamType& stream_type,
                             const std::string& key) override;
  base::Time GetLastFetchTime(const StreamType& stream_type) override;
  void SetContentOrder(const StreamType& stream_type,
                       ContentOrder content_order) override;
  ContentOrder GetContentOrder(const StreamType& stream_type) override;
  ContentOrder GetContentOrderFromPrefs(const StreamType& stream_type) override;

  // offline_pages::TaskQueue::Delegate.
  void OnTaskQueueIsIdle() override;

  // MetricsReporter::Delegate.
  void SubscribedWebFeedCount(base::OnceCallback<void(int)> callback) override;

  // StreamModel::StoreObserver.
  void OnStoreChange(StreamModel::StoreUpdate update) override;

  // Event indicators. These functions are called from an external source
  // to indicate an event.

  // Called when Chrome's EULA has been accepted. This should happen when
  // Delegate::IsEulaAccepted() changes from false to true.
  void OnEulaAccepted();
  // Invoked when Chrome is backgrounded.
  void OnEnterBackground();
  // The user signed in to Chrome.
  void OnSignedIn();
  // The user signed out of Chrome.
  void OnSignedOut();
  // The user has deleted all browsing history.
  void OnAllHistoryDeleted();
  // Chrome's cached data was cleared.
  void OnCacheDataCleared();

  // State shared for the sake of implementing FeedStream. Typically these
  // functions are used by tasks.

  void LoadModel(const StreamType& stream_type,
                 std::unique_ptr<StreamModel> model);

  // Store/upload an action and update the consistency token. |callback| is
  // called with |true| if the consistency token was written to the store.
  void UploadAction(
      feedwire::FeedAction action,
      bool upload_now,
      base::OnceCallback<void(UploadActionsTask::Result)> callback);

  FeedNetwork& GetNetwork() { return *feed_network_; }
  FeedStore& GetStore() { return *store_; }
  RequestThrottler& GetRequestThrottler() { return request_throttler_; }
  const feedstore::Metadata& GetMetadata() const;
  void SetMetadata(feedstore::Metadata metadata);
  bool SetMetadata(absl::optional<feedstore::Metadata> metadata);
  void SetStreamStale(const StreamType& stream_type, bool is_stale);

  MetricsReporter& GetMetricsReporter() const { return *metrics_reporter_; }

  void PrefetchImage(const GURL& url);

  bool IsSignedIn() const { return !delegate_->GetSyncSignedInGaia().empty(); }
  std::string GetSyncSignedInGaia() const {
    return delegate_->GetSyncSignedInGaia();
  }

  // Determines if we should attempt loading the stream or refreshing at all.
  // Returns |LoadStreamStatus::kNoStatus| if loading may be attempted.
  LaunchResult ShouldAttemptLoad(const StreamType& stream_type,
                                 LoadType load_type,
                                 bool model_loading = false);

  // Whether the last scheduled refresh was missed.
  bool MissedLastRefresh(const StreamType& stream_type);

  // Determines if a FeedQuery request can be made. If successful,
  // returns |LoadStreamStatus::kNoStatus| and acquires throttler quota.
  // Otherwise returns the reason. If |consume_quota| is false, no quota is
  // consumed. This can be used to predict the likely result on a subsequent
  // call.
  LaunchResult ShouldMakeFeedQueryRequest(const StreamType& stream_type,
                                          LoadType load_type,
                                          bool consume_quota = true);

  // Returns true if a FeedQuery request made right now should be made without
  // user credentials.
  bool ShouldForceSignedOutFeedQueryRequest(
      const StreamType& stream_type) const;

  // Unloads one stream model. Surfaces are not updated, and will remain frozen
  // until a model load is requested.
  void UnloadModel(const StreamType& stream_type);
  // Unloads all stream models.
  void UnloadModels();

  // Triggers a stream load. The load will be aborted if |ShouldAttemptLoad()|
  // is not true. Returns CARDS_UNSPECIFIED if loading is to proceed, or another
  // DiscoverLaunchResult if loading will not be attempted.
  feedwire::DiscoverLaunchResult TriggerStreamLoad(
      const StreamType& stream_type);

  // Only to be called by ClearAllTask. This clears other stream data stored in
  // memory.
  void FinishClearAll();

  // Returns the model if it is loaded, or null otherwise.
  StreamModel* GetModel(const StreamType& stream_type);

  RequestMetadata GetRequestMetadata(const StreamType& stream_type,
                                     bool is_for_next_page) const;

  bool HasUnreadContent(const StreamType& stream_type);

  bool IsOffline() const { return delegate_->IsOffline(); }

  offline_pages::TaskQueue& GetTaskQueue() { return task_queue_; }

  const WireResponseTranslator& GetWireResponseTranslator() const {
    return *wire_response_translator_;
  }

  LaunchReliabilityLogger& GetLaunchReliabilityLogger(
      const StreamType& stream_type);

  // Testing functionality.
  offline_pages::TaskQueue& GetTaskQueueForTesting();
  // Loads |model|. Should be used for testing in place of typical model
  // loading from network or storage.
  void LoadModelForTesting(const StreamType& stream_type,
                           std::unique_ptr<StreamModel> model);
  void SetWireResponseTranslatorForTesting(
      const WireResponseTranslator* wire_response_translator) {
    wire_response_translator_ = wire_response_translator;
  }
  void SetIdleCallbackForTesting(base::RepeatingClosure idle_callback);

  bool CanUploadActions() const;

  bool ClearAllInProgress() const { return clear_all_in_progress_; }

  bool IsEnabledAndVisible();

  LoggingParameters GetLoggingParameters(const StreamType& stream_type);

  base::WeakPtr<FeedStream> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  using UnreadContentNotifier = feed_stream::UnreadContentNotifier;

  struct Stream {
    explicit Stream(const StreamType& stream_type);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    StreamType type;
    // Whether the model is being loaded. Used to prevent multiple simultaneous
    // attempts to load the model.
    bool model_loading_in_progress = false;
    StreamSurfaceSet surfaces;
    std::unique_ptr<SurfaceUpdater> surface_updater;
    // The stream model. Null if not yet loaded.
    // Internally, this should only be changed by |LoadModel()| and
    // |UnloadModel()|.
    std::unique_ptr<StreamModel> model;
    int unload_on_detach_sequence_number = 0;
    ContentIdSet content_ids;
    std::vector<UnreadContentNotifier> unread_content_notifiers;
    std::vector<base::OnceCallback<void(bool)>> load_more_complete_callbacks;
    std::vector<base::OnceCallback<void(bool)>> refresh_complete_callbacks;
    bool is_activity_logging_enabled = false;
  };

  void InitializeComplete(WaitForStoreInitializeTask::Result result);

  void SetRequestSchedule(const StreamType& stream_type,
                          RequestSchedule schedule);

  void SetRequestSchedule(RefreshTaskId task_id, RequestSchedule schedule);

  // Re-evaluate whether or not activity logging should currently be enabled.
  void UpdateIsActivityLoggingEnabled(const StreamType& stream_type);

  // A single function task to delete stored feed data and force a refresh.
  // To only be called from within a |Task|.
  void ForceRefreshForDebuggingTask(const StreamType& stream_type);
  void ForceRefreshTask(const StreamType& stream_type);

  void ScheduleModelUnloadIfNoSurfacesAttached(const StreamType& stream_type);
  void AddUnloadModelIfNoSurfacesAttachedTask(const StreamType& stream_type,
                                              int sequence_number);
  void UnloadModelIfNoSurfacesAttachedTask(const StreamType& stream_type);

  void StreamLoadComplete(LoadStreamTask::Result result);
  void LoadMoreComplete(LoadMoreTask::Result result);
  void BackgroundRefreshComplete(LoadStreamTask::Result result);
  void LoadTaskComplete(const LoadStreamTask::Result& result);
  void UploadActionsComplete(UploadActionsTask::Result result);
  void ClearAll();

  bool IsFeedEnabledByEnterprisePolicy();

  bool HasReachedConditionsToUploadActionsWithNoticeCard();

  bool CanLogViews() const;

  void MaybeNotifyHasUnreadContent(const StreamType& stream_type);
  void EnabledPreferencesChanged();

  Stream& GetStream(const StreamType& type);
  Stream* FindStream(const StreamType& type);
  const Stream* FindStream(const StreamType& type) const;
  void UpdateExperiments(Experiments experiments);

  NoticeCardTracker& GetNoticeCardTracker(const std::string& key);

  // Unowned.

  raw_ptr<RefreshTaskScheduler> refresh_task_scheduler_;
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<Delegate> delegate_;
  raw_ptr<PrefService> profile_prefs_;  // May be null.
  raw_ptr<FeedNetwork> feed_network_;
  raw_ptr<ImageFetcher> image_fetcher_;
  raw_ptr<FeedStore> store_;
  raw_ptr<PersistentKeyValueStoreImpl> persistent_key_value_store_;
  raw_ptr<const WireResponseTranslator> wire_response_translator_;

  StreamModel::Context stream_model_context_;

  ChromeInfo chrome_info_;

  offline_pages::TaskQueue task_queue_;

  std::map<StreamType, Stream> streams_;

  std::unique_ptr<WebFeedSubscriptionCoordinator>
      web_feed_subscription_coordinator_;

  // Mutable state.
  RequestThrottler request_throttler_;
  base::TimeTicks signed_out_for_you_refreshes_until_;

  BooleanPrefMember has_stored_data_;
  BooleanPrefMember enable_snippets_;
  BooleanPrefMember articles_list_visible_;

  // State loaded at startup:
  feedstore::Metadata metadata_;
  bool metadata_populated_ = false;

  base::ObserverList<UnreadContentObserver> unread_content_observers_;

  // To allow tests to wait on task queue idle.
  base::RepeatingClosure idle_callback_;

  // Stream update forced to use for new surfaces. This is provided in feed
  // internals page for debugging purpose.
  feedui::StreamUpdate forced_stream_update_for_debugging_;

  feed_stream::UploadCriteria upload_criteria_;
  PrivacyNoticeCardTracker privacy_notice_card_tracker_;

  std::map<std::string, NoticeCardTracker> notice_card_trackers_;

  bool clear_all_in_progress_ = false;

  std::vector<GURL> recent_feed_navigations_;

  base::WeakPtrFactory<FeedStream> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
