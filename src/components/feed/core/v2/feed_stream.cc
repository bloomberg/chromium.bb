// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_stream_surface.h"
#include "components/feed/core/v2/public/refresh_task_scheduler.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/unread_content_observer.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream/notice_card_tracker.h"
#include "components/feed/core/v2/stream/unread_content_notifier.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/surface_updater.h"
#include "components/feed/core/v2/tasks/clear_all_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/prefetch_images_task.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/task/closure_task.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {
constexpr size_t kMaxRecentFeedNavigations = 10;

void UpdateDebugStreamData(
    const UploadActionsTask::Result& upload_actions_result,
    DebugStreamData& debug_data) {
  if (upload_actions_result.last_network_response_info) {
    debug_data.upload_info = upload_actions_result.last_network_response_info;
  }
}

void PopulateDebugStreamData(const LoadStreamTask::Result& load_result,
                             PrefService& profile_prefs) {
  DebugStreamData debug_data = ::feed::prefs::GetDebugStreamData(profile_prefs);
  std::stringstream ss;
  ss << "Code: " << load_result.final_status;
  debug_data.load_stream_status = ss.str();
  if (load_result.network_response_info) {
    debug_data.fetch_info = load_result.network_response_info;
  }
  if (load_result.upload_actions_result) {
    UpdateDebugStreamData(*load_result.upload_actions_result, debug_data);
  }
  ::feed::prefs::SetDebugStreamData(debug_data, profile_prefs);
}

void PopulateDebugStreamData(
    const UploadActionsTask::Result& upload_actions_result,
    PrefService& profile_prefs) {
  DebugStreamData debug_data = ::feed::prefs::GetDebugStreamData(profile_prefs);
  UpdateDebugStreamData(upload_actions_result, debug_data);
  ::feed::prefs::SetDebugStreamData(debug_data, profile_prefs);
}

// Will check all sources of ordering setting and always return a valid result.
ContentOrder GetValidWebFeedContentOrder(const PrefService& pref_service) {
  // First priority is the prefs stored order choice.
  ContentOrder pref_order = prefs::GetWebFeedContentOrder(pref_service);
  if (pref_order != ContentOrder::kUnspecified)
    return pref_order;
  // Fallback to Finch determined order.
  std::string finch_order = base::GetFieldTrialParamValueByFeature(
      kWebFeed, "following_feed_content_order");
  if (finch_order == "reverse_chron")
    return ContentOrder::kReverseChron;
  // Defaults to grouped, encompassing finch_order == "grouped".
  return ContentOrder::kGrouped;
}

}  // namespace

FeedStream::Stream::Stream(const StreamType& stream_type)
    : type(stream_type), surfaces(stream_type) {}
FeedStream::Stream::~Stream() = default;

FeedStream::FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
                       MetricsReporter* metrics_reporter,
                       Delegate* delegate,
                       PrefService* profile_prefs,
                       FeedNetwork* feed_network,
                       ImageFetcher* image_fetcher,
                       FeedStore* feed_store,
                       PersistentKeyValueStoreImpl* persistent_key_value_store,
                       const ChromeInfo& chrome_info)
    : refresh_task_scheduler_(refresh_task_scheduler),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate),
      profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      image_fetcher_(image_fetcher),
      store_(feed_store),
      persistent_key_value_store_(persistent_key_value_store),
      chrome_info_(chrome_info),
      task_queue_(this),
      request_throttler_(profile_prefs),
      upload_criteria_(profile_prefs),
      privacy_notice_card_tracker_(profile_prefs) {
  DCHECK(persistent_key_value_store_);
  DCHECK(feed_network_);
  DCHECK(profile_prefs_);
  DCHECK(metrics_reporter);
  DCHECK(image_fetcher_);

  static WireResponseTranslator default_translator;
  wire_response_translator_ = &default_translator;
  metrics_reporter_->Initialize(this);

  base::RepeatingClosure preference_change_callback =
      base::BindRepeating(&FeedStream::EnabledPreferencesChanged, GetWeakPtr());
  enable_snippets_.Init(prefs::kEnableSnippets, profile_prefs,
                        preference_change_callback);
  articles_list_visible_.Init(prefs::kArticlesListVisible, profile_prefs,
                              preference_change_callback);
  has_stored_data_.Init(feed::prefs::kHasStoredData, profile_prefs);

  web_feed_subscription_coordinator_ =
      std::make_unique<WebFeedSubscriptionCoordinator>(delegate, this);

  // Inserting this task first ensures that |store_| is initialized before
  // it is used.
  task_queue_.AddTask(std::make_unique<WaitForStoreInitializeTask>(
      store_, this,
      base::BindOnce(&FeedStream::InitializeComplete, base::Unretained(this))));
  EnabledPreferencesChanged();
}

FeedStream::~FeedStream() = default;

WebFeedSubscriptionCoordinator& FeedStream::subscriptions() {
  return *web_feed_subscription_coordinator_;
}

FeedStream::Stream* FeedStream::FindStream(const StreamType& stream_type) {
  auto iter = streams_.find(stream_type);
  return (iter != streams_.end()) ? &iter->second : nullptr;
}

const FeedStream::Stream* FeedStream::FindStream(
    const StreamType& stream_type) const {
  return const_cast<FeedStream*>(this)->FindStream(stream_type);
}

FeedStream::Stream& FeedStream::GetStream(const StreamType& stream_type) {
  auto iter = streams_.find(stream_type);
  if (iter != streams_.end())
    return iter->second;
  FeedStream::Stream& new_stream =
      streams_.emplace(stream_type, stream_type).first->second;
  new_stream.surface_updater =
      std::make_unique<SurfaceUpdater>(metrics_reporter_, &new_stream.surfaces);
  new_stream.surfaces.AddObserver(new_stream.surface_updater.get());
  return new_stream;
}

StreamModel* FeedStream::GetModel(const StreamType& stream_type) {
  Stream* stream = FindStream(stream_type);
  return stream ? stream->model.get() : nullptr;
}

feedwire::DiscoverLaunchResult FeedStream::TriggerStreamLoad(
    const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (stream.model || stream.model_loading_in_progress)
    return feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED;

  // If we should not load the stream, abort and send a zero-state update.
  LaunchResult do_not_attempt_reason =
      ShouldAttemptLoad(stream_type, LoadType::kInitialLoad);
  if (do_not_attempt_reason.load_stream_status != LoadStreamStatus::kNoStatus) {
    LoadStreamTask::Result result(stream_type,
                                  do_not_attempt_reason.load_stream_status);
    result.launch_result = do_not_attempt_reason.launch_result;
    StreamLoadComplete(std::move(result));
    return do_not_attempt_reason.launch_result;
  }

  stream.model_loading_in_progress = true;

  stream.surface_updater->LoadStreamStarted(/*manual_refreshing=*/false);
  LoadStreamTask::Options options;
  options.stream_type = stream_type;
  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      options, this,
      base::BindOnce(&FeedStream::StreamLoadComplete, base::Unretained(this))));
  return feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED;
}

void FeedStream::InitializeComplete(WaitForStoreInitializeTask::Result result) {
  metadata_ = *std::move(result.startup_data.metadata);
  for (const feedstore::StreamData& stream_data :
       result.startup_data.stream_data) {
    StreamType stream_type =
        feedstore::StreamTypeFromId(stream_data.stream_id());
    if (stream_type.IsValid()) {
      GetStream(stream_type).content_ids =
          feedstore::GetContentIds(stream_data);
    }
  }
  metadata_populated_ = true;
  web_feed_subscription_coordinator_->Populate(result.web_feed_startup_data);

  for (const feedstore::StreamData& stream_data :
       result.startup_data.stream_data) {
    StreamType stream_type =
        feedstore::StreamTypeFromId(stream_data.stream_id());
    if (stream_type.IsValid())
      MaybeNotifyHasUnreadContent(stream_type);
  }

  if (!IsEnabledAndVisible() && has_stored_data_.GetValue()) {
    ClearAll();
  }
}

void FeedStream::StreamLoadComplete(LoadStreamTask::Result result) {
  DCHECK(result.load_type == LoadType::kInitialLoad ||
         result.load_type == LoadType::kManualRefresh);

  Stream& stream = GetStream(result.stream_type);
  if (result.load_type == LoadType::kManualRefresh)
    UnloadModel(result.stream_type);

  // TODO(crbug.com/1268575): SetLastFetchHadNoticeCard is duplicated here to
  // ensure that the pref is updated before LoadModel(), which needs this
  // information. This is fragile, we should instead store this information
  // along with the stream.
  if (result.fetched_content_has_notice_card.has_value())
    feed::prefs::SetLastFetchHadNoticeCard(
        *profile_prefs_, *result.fetched_content_has_notice_card);

  if (result.update_request) {
    auto model = std::make_unique<StreamModel>(&stream_model_context_);
    model->Update(std::move(result.update_request));

    if (!model->HasVisibleContent() &&
        result.launch_result ==
            feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED) {
      result.launch_result =
          feedwire::DiscoverLaunchResult::NO_CARDS_RESPONSE_ERROR_ZERO_CARDS;
    }

    LoadModel(result.stream_type, std::move(model));
  }

  if (result.request_schedule)
    SetRequestSchedule(stream.type, *result.request_schedule);

  ContentStats content_stats;
  if (stream.model)
    content_stats = stream.model->GetContentStats();

  metrics_reporter_->OnLoadStream(
      stream.type, result.load_from_store_status, result.final_status,
      result.load_type == LoadType::kInitialLoad,
      result.loaded_new_content_from_network, result.stored_content_age,
      content_stats, GetRequestMetadata(stream.type, false),
      std::move(result.latencies));

  UpdateIsActivityLoggingEnabled(result.stream_type);
  stream.model_loading_in_progress = false;
  stream.surface_updater->LoadStreamComplete(
      stream.model != nullptr, result.final_status, result.launch_result);

  LoadTaskComplete(result);

  // When done loading the for-you feed, try to refresh the web-feed if there's
  // no unread content.
  if (base::FeatureList::IsEnabled(kWebFeed) &&
      result.load_type != LoadType::kManualRefresh) {
    if (result.stream_type.IsForYou()) {
      if (!HasUnreadContent(kWebFeedStream)) {
        LoadStreamTask::Options options;
        options.load_type = LoadType::kBackgroundRefresh;
        options.stream_type = kWebFeedStream;
        options.abort_if_unread_content = true;
        task_queue_.AddTask(std::make_unique<LoadStreamTask>(
            options, this,
            base::BindOnce(&FeedStream::BackgroundRefreshComplete,
                           base::Unretained(this))));
      }
    }
  }

  if (result.load_type == LoadType::kManualRefresh) {
    std::vector<base::OnceCallback<void(bool)>> moved_callbacks =
        std::move(stream.refresh_complete_callbacks);
    for (auto& callback : moved_callbacks) {
      std::move(callback).Run(result.loaded_new_content_from_network);
    }
  }
}

LoggingParameters FeedStream::GetLoggingParameters(
    const StreamType& stream_type) {
  LoggingParameters logging_params;
  logging_params.client_instance_id = GetClientInstanceId();
  logging_params.logging_enabled = IsActivityLoggingEnabled(stream_type);
  Stream& stream = GetStream(stream_type);
  if (stream.model) {
    logging_params.root_event_id = stream.model->GetRootEventId();
  }
  logging_params.view_actions_enabled = CanLogViews();
  // We provide account name even if logging is disabled, so that account name
  // can be verified for action uploads.
  logging_params.email = delegate_->GetSyncSignedInEmail();

  return logging_params;
}

void FeedStream::OnEnterBackground() {
  metrics_reporter_->OnEnterBackground();
  if (GetFeedConfig().upload_actions_on_enter_background) {
    task_queue_.AddTask(std::make_unique<UploadActionsTask>(
        this,
        /*launch_reliability_logger=*/nullptr,
        base::BindOnce(&FeedStream::UploadActionsComplete,
                       base::Unretained(this))));
  }
}

bool FeedStream::IsActivityLoggingEnabled(const StreamType& stream_type) const {
  const Stream* stream = FindStream(stream_type);
  return stream && stream->is_activity_logging_enabled && CanUploadActions();
}

void FeedStream::UpdateIsActivityLoggingEnabled(const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);

  stream.is_activity_logging_enabled =
      stream.model &&
      ((stream.model->signed_in() && stream.model->logging_enabled()) ||
       (!stream.model->signed_in() &&
        GetFeedConfig().send_signed_out_session_logs));
}

std::string FeedStream::GetSessionId() const {
  return metadata_.session_id().token();
}

const feedstore::Metadata& FeedStream::GetMetadata() const {
  DCHECK(metadata_populated_)
      << "Metadata is not yet populated. This function should only be called "
         "after the WaitForStoreInitialize task is complete.";
  return metadata_;
}

void FeedStream::SetMetadata(feedstore::Metadata metadata) {
  metadata_ = std::move(metadata);
  store_->WriteMetadata(metadata_, base::DoNothing());
}

void FeedStream::SetStreamStale(const StreamType& stream_type, bool is_stale) {
  feedstore::Metadata metadata = GetMetadata();
  feedstore::Metadata::StreamMetadata& stream_metadata =
      feedstore::MetadataForStream(metadata, stream_type);
  if (stream_metadata.is_known_stale() != is_stale) {
    stream_metadata.set_is_known_stale(is_stale);
    if (is_stale) {
      SetStreamViewContentIds(metadata_, stream_type, {});
    }
    SetMetadata(metadata);
  }
}

bool FeedStream::SetMetadata(absl::optional<feedstore::Metadata> metadata) {
  if (metadata) {
    SetMetadata(std::move(*metadata));
    return true;
  }
  return false;
}

void FeedStream::PrefetchImage(const GURL& url) {
  delegate_->PrefetchImage(url);
}

void FeedStream::UpdateExperiments(Experiments experiments) {
  delegate_->RegisterExperiments(experiments);
  prefs::SetExperiments(experiments, *profile_prefs_);
}

void FeedStream::AttachSurface(FeedStreamSurface* surface) {
  metrics_reporter_->SurfaceOpened(surface->GetStreamType(),
                                   surface->GetSurfaceId());
  Stream& stream = GetStream(surface->GetStreamType());
  // Skip normal processing when overriding stream data from the internals page.
  if (forced_stream_update_for_debugging_.updated_slices_size() > 0) {
    stream.surfaces.SurfaceAdded(surface,
                                 /*loading_not_allowed_reason=*/feedwire::
                                     DiscoverLaunchResult::CARDS_UNSPECIFIED);
    surface->StreamUpdate(forced_stream_update_for_debugging_);
    return;
  }

  stream.surfaces.SurfaceAdded(surface,
                               TriggerStreamLoad(surface->GetStreamType()));

  // Cancel any scheduled model unload task.
  ++stream.unload_on_detach_sequence_number;
  upload_criteria_.SurfaceOpenedOrClosed();
}

void FeedStream::DetachSurface(FeedStreamSurface* surface) {
  Stream& stream = GetStream(surface->GetStreamType());
  metrics_reporter_->SurfaceClosed(surface->GetSurfaceId());
  stream.surfaces.SurfaceRemoved(surface);
  upload_criteria_.SurfaceOpenedOrClosed();
  ScheduleModelUnloadIfNoSurfacesAttached(surface->GetStreamType());
}

void FeedStream::AddUnreadContentObserver(const StreamType& stream_type,
                                          UnreadContentObserver* observer) {
  GetStream(stream_type)
      .unread_content_notifiers.emplace_back(observer->GetWeakPtr());
  MaybeNotifyHasUnreadContent(stream_type);
}

void FeedStream::RemoveUnreadContentObserver(const StreamType& stream_type,
                                             UnreadContentObserver* observer) {
  Stream& stream = GetStream(stream_type);
  auto predicate = [&](const UnreadContentNotifier& notifier) {
    UnreadContentObserver* ptr = notifier.observer().get();
    return ptr == nullptr || observer == ptr;
  };
  base::EraseIf(stream.unread_content_notifiers, predicate);
}

void FeedStream::ScheduleModelUnloadIfNoSurfacesAttached(
    const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (!stream.surfaces.empty())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FeedStream::AddUnloadModelIfNoSurfacesAttachedTask,
                     GetWeakPtr(), stream.type,
                     stream.unload_on_detach_sequence_number),
      GetFeedConfig().model_unload_timeout);
}

void FeedStream::AddUnloadModelIfNoSurfacesAttachedTask(
    const StreamType& stream_type,
    int sequence_number) {
  Stream& stream = GetStream(stream_type);
  // Don't continue if unload_on_detach_sequence_number_ has changed.
  if (stream.unload_on_detach_sequence_number != sequence_number)
    return;
  task_queue_.AddTask(std::make_unique<offline_pages::ClosureTask>(
      base::BindOnce(&FeedStream::UnloadModelIfNoSurfacesAttachedTask,
                     base::Unretained(this), stream_type)));
}

void FeedStream::UnloadModelIfNoSurfacesAttachedTask(
    const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (!stream.surfaces.empty())
    return;
  UnloadModel(stream_type);
}

bool FeedStream::IsArticlesListVisible() {
  return articles_list_visible_.GetValue();
}

std::string FeedStream::GetClientInstanceId() const {
  return prefs::GetClientInstanceId(*profile_prefs_);
}

bool FeedStream::IsFeedEnabledByEnterprisePolicy() {
  return enable_snippets_.GetValue();
}

bool FeedStream::IsEnabledAndVisible() {
  return IsArticlesListVisible() && IsFeedEnabledByEnterprisePolicy();
}

void FeedStream::EnabledPreferencesChanged() {
  // Assume there might be stored data if the Feed is ever enabled.
  if (IsEnabledAndVisible())
    has_stored_data_.SetValue(true);
}

void FeedStream::LoadMore(const FeedStreamSurface& surface,
                          base::OnceCallback<void(bool)> callback) {
  Stream& stream = GetStream(surface.GetStreamType());
  if (!stream.model) {
    DLOG(ERROR) << "Ignoring LoadMore() before the model is loaded";
    return std::move(callback).Run(false);
  }
  // We want to abort early to avoid showing a loading spinner if it's not
  // necessary.
  if (ShouldMakeFeedQueryRequest(surface.GetStreamType(), LoadType::kLoadMore,
                                 /*consume_quota=*/false)
          .load_stream_status != LoadStreamStatus::kNoStatus) {
    return std::move(callback).Run(false);
  }

  metrics_reporter_->OnLoadMoreBegin(surface.GetStreamType(),
                                     surface.GetSurfaceId());
  stream.surface_updater->SetLoadingMore(true);

  // Have at most one in-flight LoadMore() request per stream. Send the result
  // to all requestors.
  stream.load_more_complete_callbacks.push_back(std::move(callback));
  if (stream.load_more_complete_callbacks.size() == 1) {
    task_queue_.AddTask(std::make_unique<LoadMoreTask>(
        surface.GetStreamType(), this,
        base::BindOnce(&FeedStream::LoadMoreComplete, base::Unretained(this))));
  }
}

void FeedStream::LoadMoreComplete(LoadMoreTask::Result result) {
  Stream& stream = GetStream(result.stream_type);
  if (stream.model && result.model_update_request)
    stream.model->Update(std::move(result.model_update_request));

  if (result.request_schedule)
    SetRequestSchedule(stream.type, *result.request_schedule);

  UpdateIsActivityLoggingEnabled(stream.type);
  metrics_reporter_->OnLoadMore(
      result.stream_type, result.final_status,
      stream.model ? stream.model->GetContentStats() : ContentStats());
  stream.surface_updater->SetLoadingMore(false);
  std::vector<base::OnceCallback<void(bool)>> moved_callbacks =
      std::move(stream.load_more_complete_callbacks);
  bool success = result.final_status == LoadStreamStatus::kLoadedFromNetwork;
  for (auto& callback : moved_callbacks) {
    std::move(callback).Run(success);
  }
}

void FeedStream::ManualRefresh(const StreamType& stream_type,
                               base::OnceCallback<void(bool)> callback) {
  Stream& stream = GetStream(stream_type);

  // Bail out immediately if loading in progress.
  if (stream.model_loading_in_progress) {
    return std::move(callback).Run(false);
  }
  stream.model_loading_in_progress = true;

  stream.surface_updater->LoadStreamStarted(/*manual_refreshing=*/true);

  // Have at most one in-flight refresh request per stream.
  stream.refresh_complete_callbacks.push_back(std::move(callback));
  if (stream.refresh_complete_callbacks.size() == 1) {
    LoadStreamTask::Options options;
    options.stream_type = stream_type;
    options.load_type = LoadType::kManualRefresh;
    task_queue_.AddTask(std::make_unique<LoadStreamTask>(
        options, this,
        base::BindOnce(&FeedStream::StreamLoadComplete,
                       base::Unretained(this))));
  }
}

void FeedStream::ExecuteOperations(
    const StreamType& stream_type,
    std::vector<feedstore::DataOperation> operations) {
  StreamModel* model = GetModel(stream_type);
  if (!model) {
    DLOG(ERROR) << "Calling ExecuteOperations before the model is loaded";
    return;
  }
  // TODO(crbug.com/1227897): Convert this to a task.
  return model->ExecuteOperations(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChange(
    const StreamType& stream_type,
    std::vector<feedstore::DataOperation> operations) {
  StreamModel* model = GetModel(stream_type);
  if (!model) {
    DLOG(ERROR) << "Calling CreateEphemeralChange before the model is loaded";
    return {};
  }
  metrics_reporter_->OtherUserAction(stream_type,
                                     FeedUserActionType::kEphemeralChange);
  return model->CreateEphemeralChange(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChangeFromPackedData(
    const StreamType& stream_type,
    base::StringPiece data) {
  feedpacking::DismissData msg;
  msg.ParseFromArray(data.data(), data.size());
  return CreateEphemeralChange(stream_type,
                               TranslateDismissData(base::Time::Now(), msg));
}

bool FeedStream::CommitEphemeralChange(const StreamType& stream_type,
                                       EphemeralChangeId id) {
  StreamModel* model = GetModel(stream_type);
  if (!model)
    return false;
  metrics_reporter_->OtherUserAction(
      stream_type, FeedUserActionType::kEphemeralChangeCommited);
  return model->CommitEphemeralChange(id);
}

bool FeedStream::RejectEphemeralChange(const StreamType& stream_type,
                                       EphemeralChangeId id) {
  StreamModel* model = GetModel(stream_type);
  if (!model)
    return false;
  metrics_reporter_->OtherUserAction(
      stream_type, FeedUserActionType::kEphemeralChangeRejected);
  return model->RejectEphemeralChange(id);
}

void FeedStream::ProcessThereAndBackAgain(base::StringPiece data) {
  feedwire::ThereAndBackAgainData msg;
  msg.ParseFromArray(data.data(), data.size());
  if (msg.has_action_payload()) {
    feedwire::FeedAction action_msg;
    *action_msg.mutable_action_payload() = std::move(msg.action_payload());
    UploadAction(std::move(action_msg), /*upload_now=*/true,
                 base::BindOnce(&FeedStream::UploadActionsComplete,
                                base::Unretained(this)));
  }
}

void FeedStream::ProcessThereAndBackAgain(
    base::StringPiece data,
    const feedui::LoggingParameters& logging_parameters) {
  // TODO(crbug.com/1268575): Thread logging parameters to UploadActionTask when
  // it's always available.
  ProcessThereAndBackAgain(data);
}

void FeedStream::ProcessViewAction(base::StringPiece data) {
  if (!CanLogViews()) {
    return;
  }

  feedwire::FeedAction msg;
  msg.ParseFromArray(data.data(), data.size());
  UploadAction(std::move(msg), /*upload_now=*/false,
               base::BindOnce(&FeedStream::UploadActionsComplete,
                              base::Unretained(this)));
}

void FeedStream::ProcessViewAction(
    base::StringPiece data,
    const feedui::LoggingParameters& logging_parameters) {
  // TODO(crbug.com/1268575): Thread logging parameters to UploadActionTask when
  // it's always available.
  ProcessViewAction(data);
}

void FeedStream::UploadActionsComplete(UploadActionsTask::Result result) {
  PopulateDebugStreamData(result, *profile_prefs_);
}

bool FeedStream::WasUrlRecentlyNavigatedFromFeed(const GURL& url) {
  return std::find(recent_feed_navigations_.begin(),
                   recent_feed_navigations_.end(),
                   url) != recent_feed_navigations_.end();
}

DebugStreamData FeedStream::GetDebugStreamData() {
  return ::feed::prefs::GetDebugStreamData(*profile_prefs_);
}

void FeedStream::ForceRefreshForDebugging(const StreamType& stream_type) {
  // Avoid request throttling for debug refreshes.
  feed::prefs::SetThrottlerRequestCounts({}, *profile_prefs_);
  task_queue_.AddTask(std::make_unique<offline_pages::ClosureTask>(
      base::BindOnce(&FeedStream::ForceRefreshForDebuggingTask,
                     base::Unretained(this), stream_type)));
}

void FeedStream::ForceRefreshTask(const StreamType& stream_type) {
  UnloadModel(stream_type);
  store_->ClearStreamData(stream_type, base::DoNothing());
  GetStream(stream_type)
      .surface_updater->launch_reliability_logger()
      .LogFeedLaunchOtherStart();
  if (!GetStream(stream_type).surfaces.empty())
    TriggerStreamLoad(stream_type);
}

void FeedStream::ForceRefreshForDebuggingTask(const StreamType& stream_type) {
  UnloadModel(stream_type);
  store_->ClearStreamData(stream_type, base::DoNothing());
  GetStream(stream_type)
      .surface_updater->launch_reliability_logger()
      .LogFeedLaunchOtherStart();
  TriggerStreamLoad(stream_type);
}

std::string FeedStream::DumpStateForDebugging() {
  Stream& stream = GetStream(kForYouStream);
  std::stringstream ss;
  if (stream.model) {
    ss << "model loaded, " << stream.model->GetContentList().size()
       << " contents, "
       << "signed_in=" << stream.model->signed_in()
       << ", logging_enabled=" << stream.model->logging_enabled()
       << ", privacy_notice_fulfilled="
       << stream.model->privacy_notice_fulfilled();
  }

  auto print_refresh_schedule = [&](RefreshTaskId task_id) {
    RequestSchedule schedule =
        prefs::GetRequestSchedule(task_id, *profile_prefs_);
    if (schedule.refresh_offsets.empty()) {
      ss << "No request schedule\n";
    } else {
      ss << "Request schedule reference " << schedule.anchor_time << '\n';
      for (base::TimeDelta entry : schedule.refresh_offsets) {
        ss << " fetch at " << entry << '\n';
      }
    }
  };
  ss << "For You: ";
  print_refresh_schedule(RefreshTaskId::kRefreshForYouFeed);
  ss << "WebFeeds: ";
  print_refresh_schedule(RefreshTaskId::kRefreshWebFeed);
  ss << "WebFeedSubscriptions:\n";
  subscriptions().DumpStateForDebugging(ss);
  return ss.str();
}

void FeedStream::SetForcedStreamUpdateForDebugging(
    const feedui::StreamUpdate& stream_update) {
  forced_stream_update_for_debugging_ = stream_update;
}

base::Time FeedStream::GetLastFetchTime(const StreamType& stream_type) {
  const base::Time fetch_time =
      feedstore::GetLastFetchTime(metadata_, stream_type);
  // Ignore impossible time values.
  if (fetch_time > base::Time::Now())
    return base::Time();
  return fetch_time;
}

void FeedStream::LoadModelForTesting(const StreamType& stream_type,
                                     std::unique_ptr<StreamModel> model) {
  LoadModel(stream_type, std::move(model));
}
offline_pages::TaskQueue& FeedStream::GetTaskQueueForTesting() {
  return task_queue_;
}

void FeedStream::OnTaskQueueIsIdle() {
  if (idle_callback_)
    idle_callback_.Run();
}

void FeedStream::SubscribedWebFeedCount(
    base::OnceCallback<void(int)> callback) {
  subscriptions().SubscribedWebFeedCount(std::move(callback));
}

void FeedStream::SetIdleCallbackForTesting(
    base::RepeatingClosure idle_callback) {
  idle_callback_ = idle_callback;
}

void FeedStream::OnStoreChange(StreamModel::StoreUpdate update) {
  if (!update.operations.empty()) {
    DCHECK(!update.update_request);
    store_->WriteOperations(update.stream_type, update.sequence_number,
                            update.operations);
  } else {
    DCHECK(update.update_request);
    if (update.overwrite_stream_data) {
      DCHECK_EQ(update.sequence_number, 0);
      store_->OverwriteStream(update.stream_type,
                              std::move(update.update_request),
                              base::DoNothing());
    } else {
      store_->SaveStreamUpdate(update.stream_type, update.sequence_number,
                               std::move(update.update_request),
                               base::DoNothing());
    }
  }
}

LaunchResult FeedStream::ShouldAttemptLoad(const StreamType& stream_type,
                                           LoadType load_type,
                                           bool model_loading) {
  Stream& stream = GetStream(stream_type);
  if (load_type == LoadType::kInitialLoad ||
      load_type == LoadType::kBackgroundRefresh) {
    // For initial load or background refresh, the model should not be loaded
    // or in the process of being loaded. Because |ShouldAttemptLoad()| is used
    // both before and during the load process, we need to ignore this check
    // when |model_loading| is true.
    if (stream.model || (!model_loading && stream.model_loading_in_progress)) {
      return {LoadStreamStatus::kModelAlreadyLoaded,
              feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
    }
  }

  if (!IsArticlesListVisible()) {
    return {LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            feedwire::DiscoverLaunchResult::FEED_HIDDEN};
  }

  if (!IsFeedEnabledByEnterprisePolicy()) {
    return {LoadStreamStatus::kLoadNotAllowedDisabledByEnterprisePolicy,
            feedwire::DiscoverLaunchResult::
                INELIGIBLE_DISCOVER_DISABLED_BY_ENTERPRISE_POLICY};
  }

  if (!delegate_->IsEulaAccepted()) {
    return {LoadStreamStatus::kLoadNotAllowedEulaNotAccepted,
            feedwire::DiscoverLaunchResult::INELIGIBLE_EULA_NOT_ACCEPTED};
  }

  // Skip this check if metadata_ is not initialized. ShouldAttemptLoad() will
  // be called again from within the LoadStreamTask, and then the metadata
  // will be initialized.
  if (metadata_populated_ &&
      delegate_->GetSyncSignedInGaia() != metadata_.gaia()) {
    return {LoadStreamStatus::kDataInStoreIsForAnotherUser,
            feedwire::DiscoverLaunchResult::DATA_IN_STORE_IS_FOR_ANOTHER_USER};
  }

  return {LoadStreamStatus::kNoStatus,
          feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
}

bool FeedStream::MissedLastRefresh(const StreamType& stream_type) {
  RefreshTaskId task_id;
  if (!stream_type.GetRefreshTaskId(task_id))
    return false;
  RequestSchedule schedule =
      feed::prefs::GetRequestSchedule(task_id, *profile_prefs_);
  if (schedule.refresh_offsets.empty())
    return false;
  base::Time scheduled_time =
      schedule.anchor_time + schedule.refresh_offsets[0];
  return scheduled_time < base::Time::Now();
}

LaunchResult FeedStream::ShouldMakeFeedQueryRequest(
    const StreamType& stream_type,
    LoadType load_type,
    bool consume_quota) {
  Stream& stream = GetStream(stream_type);
  if (load_type == LoadType::kLoadMore) {
    // LoadMore requires a next page token.
    if (!stream.model || stream.model->GetNextPageToken().empty()) {
      return {LoadStreamStatus::kCannotLoadMoreNoNextPageToken,
              feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
    }
  } else if (load_type != LoadType::kManualRefresh) {
    // Time has passed since calling |ShouldAttemptLoad()|, call it again to
    // confirm we should still attempt loading.
    const LaunchResult should_not_attempt_reason =
        ShouldAttemptLoad(stream_type, load_type, /*model_loading=*/true);
    if (should_not_attempt_reason.load_stream_status !=
        LoadStreamStatus::kNoStatus) {
      return should_not_attempt_reason;
    }
  }

  if (delegate_->IsOffline()) {
    return {LoadStreamStatus::kCannotLoadFromNetworkOffline,
            feedwire::DiscoverLaunchResult::NO_CARDS_REQUEST_ERROR_NO_INTERNET};
  }

  if (consume_quota &&
      !request_throttler_.RequestQuota((load_type != LoadType::kLoadMore)
                                           ? NetworkRequestType::kFeedQuery
                                           : NetworkRequestType::kNextPage)) {
    return {LoadStreamStatus::kCannotLoadFromNetworkThrottled,
            feedwire::DiscoverLaunchResult::NO_CARDS_REQUEST_ERROR_OTHER};
  }

  return {LoadStreamStatus::kNoStatus,
          feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
}

bool FeedStream::ShouldForceSignedOutFeedQueryRequest(
    const StreamType& stream_type) const {
  return stream_type.IsForYou() &&
         base::TimeTicks::Now() < signed_out_for_you_refreshes_until_;
}

RequestMetadata FeedStream::GetRequestMetadata(const StreamType& stream_type,
                                               bool is_for_next_page) const {
  const Stream* stream = FindStream(stream_type);
  DCHECK(stream);
  RequestMetadata result;
  result.chrome_info = chrome_info_;
  result.display_metrics = delegate_->GetDisplayMetrics();
  result.language_tag = delegate_->GetLanguageTag();
  result.notice_card_acknowledged =
      privacy_notice_card_tracker_.HasAcknowledgedNoticeCard();
  result.autoplay_enabled = delegate_->IsAutoplayEnabled();
  result.acknowledged_notice_keys =
      NoticeCardTracker::GetAllAckowledgedKeys(profile_prefs_);
  if (stream_type.IsWebFeed()) {
    result.content_order = GetValidWebFeedContentOrder(*profile_prefs_);
  }

  if (is_for_next_page) {
    // If we are continuing an existing feed, use whatever session continuity
    // mechanism is currently associated with the stream: client-instance-id
    // for signed-in feed, session_id token for signed-out.
    DCHECK(stream->model);
    if (stream->model->signed_in()) {
      result.client_instance_id = GetClientInstanceId();
    } else {
      result.session_id = GetSessionId();
    }
  } else {
    // The request is for the first page of the feed. Use client_instance_id
    // for signed in requests and session_id token (if any, and not expired)
    // for signed-out.
    if (IsSignedIn() && !ShouldForceSignedOutFeedQueryRequest(stream_type)) {
      result.client_instance_id = GetClientInstanceId();
    } else if (!GetSessionId().empty() && feedstore::GetSessionIdExpiryTime(
                                              metadata_) > base::Time::Now()) {
      result.session_id = GetSessionId();
    }
  }

  DCHECK(result.session_id.empty() || result.client_instance_id.empty());

  return result;
}

void FeedStream::OnEulaAccepted() {
  for (auto& item : streams_) {
    if (!item.second.surfaces.empty()) {
      item.second.surface_updater->launch_reliability_logger()
          .LogFeedLaunchOtherStart();
      TriggerStreamLoad(item.second.type);
    }
  }
}

void FeedStream::OnAllHistoryDeleted() {
  // Give sync the time to propagate the changes in history to the server.
  // In the interim, only send signed-out FeedQuery requests.
  signed_out_for_you_refreshes_until_ =
      base::TimeTicks::Now() + kSuppressRefreshDuration;
  // We don't really need to delete kWebFeedStream data here, but clearing all
  // data because it's easy.
  ClearAll();
}

void FeedStream::OnCacheDataCleared() {
  ClearAll();
}

void FeedStream::OnSignedIn() {
  // On sign-in, turn off activity logging. This avoids the possibility that we
  // send logs with the wrong user info attached, but may cause us to lose
  // buffered events.
  for (auto& item : streams_) {
    item.second.is_activity_logging_enabled = false;
  }

  ClearAll();
}

void FeedStream::OnSignedOut() {
  // On sign-out, turn off activity logging. This avoids the possibility that we
  // send logs with the wrong user info attached, but may cause us to lose
  // buffered events.
  for (auto& item : streams_) {
    item.second.is_activity_logging_enabled = false;
  }

  ClearAll();
}

void FeedStream::ExecuteRefreshTask(RefreshTaskId task_id) {
  StreamType stream_type = StreamType::ForTaskId(task_id);
  LoadStreamStatus do_not_attempt_reason =
      ShouldAttemptLoad(stream_type, LoadType::kBackgroundRefresh)
          .load_stream_status;

  // If `do_not_attempt_reason` indicates the stream shouldn't be loaded, it's
  // unlikely that criteria will change, so we skip rescheduling.
  if (do_not_attempt_reason == LoadStreamStatus::kNoStatus ||
      do_not_attempt_reason == LoadStreamStatus::kModelAlreadyLoaded) {
    // Schedule the next refresh attempt. If a new refresh schedule is returned
    // through this refresh, it will be overwritten.
    SetRequestSchedule(
        task_id, feed::prefs::GetRequestSchedule(task_id, *profile_prefs_));
  }

  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    BackgroundRefreshComplete(
        LoadStreamTask::Result(stream_type, do_not_attempt_reason));
    return;
  }

  LoadStreamTask::Options options;
  options.stream_type = stream_type;
  options.load_type = LoadType::kBackgroundRefresh;
  options.refresh_even_when_not_stale = true;
  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      options, this,
      base::BindOnce(&FeedStream::BackgroundRefreshComplete,
                     base::Unretained(this))));
}

void FeedStream::BackgroundRefreshComplete(LoadStreamTask::Result result) {
  metrics_reporter_->OnBackgroundRefresh(result.stream_type,
                                         result.final_status);

  LoadTaskComplete(result);

  // Add prefetch images to task queue without waiting to finish
  // since we treat them as best-effort.
  if (result.stream_type.IsForYou())
    task_queue_.AddTask(std::make_unique<PrefetchImagesTask>(this));

  RefreshTaskId task_id;
  if (result.stream_type.GetRefreshTaskId(task_id)) {
    refresh_task_scheduler_->RefreshTaskComplete(task_id);
  }
}

// Performs work that is necessary for both background and foreground load
// tasks.
void FeedStream::LoadTaskComplete(const LoadStreamTask::Result& result) {
  if (delegate_->GetSyncSignedInGaia() != metadata_.gaia()) {
    ClearAll();
    return;
  }
  PopulateDebugStreamData(result, *profile_prefs_);
  if (result.fetched_content_has_notice_card.has_value())
    feed::prefs::SetLastFetchHadNoticeCard(
        *profile_prefs_, *result.fetched_content_has_notice_card);
  if (!result.content_ids.IsEmpty()) {
    GetStream(result.stream_type).content_ids = result.content_ids;
  }
  if (result.loaded_new_content_from_network) {
    SetStreamStale(result.stream_type, false);
    if (result.stream_type.IsForYou())
      UpdateExperiments(result.experiments);
  }

  MaybeNotifyHasUnreadContent(result.stream_type);
}

bool FeedStream::HasUnreadContent(const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (stream.content_ids.IsEmpty())
    return false;
  if (feedstore::GetViewContentIds(metadata_, stream_type)
          .ContainsAllOf(stream.content_ids)) {
    return false;
  }

  // If there is currently a surface already viewing the content, update the
  // ViewContentIds to whatever the current set is. This can happen if the
  // surface already shown is refreshed.
  if (stream.model && stream.surfaces.HasSurfaceShowingContent()) {
    SetMetadata(SetStreamViewContentIds(metadata_, stream_type,
                                        stream.model->GetContentIds()));
    return false;
  }
  return true;
}

void FeedStream::ClearAll() {
  metrics_reporter_->OnClearAll(base::Time::Now() -
                                GetLastFetchTime(kForYouStream));
  clear_all_in_progress_ = true;
  task_queue_.AddTask(std::make_unique<ClearAllTask>(this));
}

void FeedStream::FinishClearAll() {
  // Clear any experiments stored.
  has_stored_data_.SetValue(false);
  feed::prefs::SetExperiments({}, *profile_prefs_);
  feed::prefs::ClearClientInstanceId(*profile_prefs_);
  upload_criteria_.Clear();
  SetMetadata(feedstore::MakeMetadata(delegate_->GetSyncSignedInGaia()));

  delegate_->ClearAll();

  clear_all_in_progress_ = false;

  for (auto& item : streams_) {
    if (!item.second.surfaces.empty()) {
      item.second.surface_updater->launch_reliability_logger()
          .LogFeedLaunchOtherStart();
      TriggerStreamLoad(item.second.type);
    }
  }
  web_feed_subscription_coordinator_->ClearAllFinished();
}

ImageFetchId FeedStream::FetchImage(
    const GURL& url,
    base::OnceCallback<void(NetworkResponse)> callback) {
  return image_fetcher_->Fetch(url, std::move(callback));
}

PersistentKeyValueStoreImpl& FeedStream::GetPersistentKeyValueStore() {
  return *persistent_key_value_store_;
}

void FeedStream::CancelImageFetch(ImageFetchId id) {
  image_fetcher_->Cancel(id);
}

void FeedStream::UploadAction(
    feedwire::FeedAction action,
    bool upload_now,
    base::OnceCallback<void(UploadActionsTask::Result)> callback) {
  if (!IsSignedIn()) {
    DLOG(WARNING)
        << "Called UploadActions while user is signed-out, dropping upload";
    return;
  }
  task_queue_.AddTask(std::make_unique<UploadActionsTask>(
      std::move(action), upload_now, this, std::move(callback)));
}

void FeedStream::LoadModel(const StreamType& stream_type,
                           std::unique_ptr<StreamModel> model) {
  Stream& stream = GetStream(stream_type);
  DCHECK(!stream.model);
  stream.model = std::move(model);
  stream.model->SetStreamType(stream_type);
  stream.model->SetStoreObserver(this);

  // TODO(crbug.com/1268575): Once the internal changes to support per-item
  // logging parameters is submitted, we should remove
  // UpdateIsActivityLoggingEnabled() and instead store the logging parameters
  // on the model.
  UpdateIsActivityLoggingEnabled(stream_type);

  stream.content_ids = stream.model->GetContentIds();
  stream.surface_updater->SetModel(stream.model.get(),
                                   GetLoggingParameters(stream_type));
  ScheduleModelUnloadIfNoSurfacesAttached(stream_type);
  MaybeNotifyHasUnreadContent(stream_type);
}

void FeedStream::SetRequestSchedule(const StreamType& stream_type,
                                    RequestSchedule schedule) {
  RefreshTaskId task_id;
  if (!stream_type.GetRefreshTaskId(task_id)) {
    DLOG(ERROR) << "Ignoring request schedule for this stream: " << stream_type;
    return;
  }
  SetRequestSchedule(task_id, std::move(schedule));
}

void FeedStream::SetRequestSchedule(RefreshTaskId task_id,
                                    RequestSchedule schedule) {
  const base::Time now = base::Time::Now();
  base::Time run_time = NextScheduledRequestTime(now, &schedule);
  if (!run_time.is_null()) {
    refresh_task_scheduler_->EnsureScheduled(task_id, run_time - now);
  } else {
    refresh_task_scheduler_->Cancel(task_id);
  }
  feed::prefs::SetRequestSchedule(task_id, schedule, *profile_prefs_);
}

void FeedStream::UnloadModel(const StreamType& stream_type) {
  // Note: This should only be called from a running Task, as some tasks assume
  // the model remains loaded.
  Stream* stream = FindStream(stream_type);
  if (!stream || !stream->model)
    return;
  stream->surface_updater->SetModel(nullptr, LoggingParameters());
  stream->model.reset();
}

void FeedStream::UnloadModels() {
  for (auto& item : streams_) {
    UnloadModel(item.second.type);
  }
}

LaunchReliabilityLogger& FeedStream::GetLaunchReliabilityLogger(
    const StreamType& stream_type) {
  return GetStream(stream_type).surface_updater->launch_reliability_logger();
}

void FeedStream::ReportOpenAction(const GURL& url,
                                  const StreamType& stream_type,
                                  const std::string& slice_id) {
  recent_feed_navigations_.insert(recent_feed_navigations_.begin(), url);
  recent_feed_navigations_.resize(
      std::min(kMaxRecentFeedNavigations, recent_feed_navigations_.size()));

  Stream& stream = GetStream(stream_type);

  int index = stream.surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index < 0)
    index = MetricsReporter::kUnknownCardIndex;
  metrics_reporter_->OpenAction(stream_type, index);

  if (stream.model) {
    privacy_notice_card_tracker_.OnOpenAction(
        stream.model->FindContentId(ToContentRevision(slice_id)));
  }
}
void FeedStream::ReportOpenVisitComplete(base::TimeDelta visit_time) {
  metrics_reporter_->OpenVisitComplete(visit_time);
}
void FeedStream::ReportOpenInNewTabAction(const GURL& url,
                                          const StreamType& stream_type,
                                          const std::string& slice_id) {
  recent_feed_navigations_.insert(recent_feed_navigations_.begin(), url);
  recent_feed_navigations_.resize(
      std::min(kMaxRecentFeedNavigations, recent_feed_navigations_.size()));

  Stream& stream = GetStream(stream_type);
  int index = stream.surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index < 0)
    index = MetricsReporter::kUnknownCardIndex;
  metrics_reporter_->OpenInNewTabAction(stream_type, index);

  if (stream.model) {
    privacy_notice_card_tracker_.OnOpenAction(
        stream.model->FindContentId(ToContentRevision(slice_id)));
  }
}

void FeedStream::ReportSliceViewed(SurfaceId surface_id,
                                   const StreamType& stream_type,
                                   const std::string& slice_id) {
  Stream& stream = GetStream(stream_type);
  int index = stream.surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index < 0)
    return;

  if (!stream.model)
    return;

  metrics_reporter_->ContentSliceViewed(stream_type, index,
                                        stream.model->GetContentList().size());

  privacy_notice_card_tracker_.OnCardViewed(
      stream.model->signed_in(),
      stream.model->FindContentId(ToContentRevision(slice_id)));
}

// TODO(crbug/1147237): Rename this method and related members?
bool FeedStream::CanUploadActions() const {
  return upload_criteria_.CanUploadActions();
}

bool FeedStream::CanLogViews() const {
  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
  return CanUploadActions();
}

// Notifies observers if 'HasUnreadContent' has changed for `stream_type`.
// Stream content has been seen if StreamData::content_hash ==
// Metadata::StreamMetadata::view_content_hash. This should be called:
// when initial metadata is loaded, when the model is loaded, when a refresh is
// attempted, and when content is viewed.
void FeedStream::MaybeNotifyHasUnreadContent(const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (!metadata_populated_ || stream.model_loading_in_progress)
    return;

  const bool has_new_content = HasUnreadContent(stream_type);
  for (auto& o : stream.unread_content_notifiers) {
    o.NotifyIfValueChanged(has_new_content);
  }
}

void FeedStream::ReportFeedViewed(const StreamType& stream_type,
                                  SurfaceId surface_id) {
  metrics_reporter_->FeedViewed(surface_id);

  Stream& stream = GetStream(stream_type);
  stream.surfaces.FeedViewed(surface_id);

  MaybeNotifyHasUnreadContent(stream_type);
}

void FeedStream::ReportPageLoaded() {
  metrics_reporter_->PageLoaded();
}
void FeedStream::ReportStreamScrolled(const StreamType& stream_type,
                                      int distance_dp) {
  metrics_reporter_->StreamScrolled(stream_type, distance_dp);
}
void FeedStream::ReportStreamScrollStart() {
  metrics_reporter_->StreamScrollStart();
}
void FeedStream::ReportOtherUserAction(const StreamType& stream_type,
                                       FeedUserActionType action_type) {
  metrics_reporter_->OtherUserAction(stream_type, action_type);
}

void FeedStream::ReportNoticeCreated(const StreamType& stream_type,
                                     const std::string& key) {
  metrics_reporter_->OnNoticeCreated(stream_type, key);
}

void FeedStream::ReportNoticeViewed(const StreamType& stream_type,
                                    const std::string& key) {
  metrics_reporter_->OnNoticeViewed(stream_type, key);
  NoticeCardTracker& tracker = GetNoticeCardTracker(key);
  bool was_acknowledged = tracker.HasAcknowledged();
  tracker.OnViewed();
  if (!was_acknowledged && tracker.HasAcknowledged()) {
    metrics_reporter_->OnNoticeAcknowledged(
        stream_type, key, NoticeAcknowledgementPath::kViaViewing);
  }
}

void FeedStream::ReportNoticeOpenAction(const StreamType& stream_type,
                                        const std::string& key) {
  metrics_reporter_->OnNoticeOpenAction(stream_type, key);
  NoticeCardTracker& tracker = GetNoticeCardTracker(key);
  bool was_acknowledged = tracker.HasAcknowledged();
  tracker.OnOpenAction();
  if (!was_acknowledged && tracker.HasAcknowledged())
    metrics_reporter_->OnNoticeAcknowledged(
        stream_type, key, NoticeAcknowledgementPath::kViaOpenAction);
}

void FeedStream::ReportNoticeDismissed(const StreamType& stream_type,
                                       const std::string& key) {
  metrics_reporter_->OnNoticeDismissed(stream_type, key);
  NoticeCardTracker& tracker = GetNoticeCardTracker(key);
  bool was_acknowledged = tracker.HasAcknowledged();
  tracker.OnDismissed();
  if (!was_acknowledged && tracker.HasAcknowledged())
    metrics_reporter_->OnNoticeAcknowledged(
        stream_type, key, NoticeAcknowledgementPath::kViaDismissal);
}

NoticeCardTracker& FeedStream::GetNoticeCardTracker(const std::string& key) {
  const auto iter = notice_card_trackers_.find(key);
  if (iter != notice_card_trackers_.end())
    return iter->second;

  return notice_card_trackers_
      .emplace(std::piecewise_construct, std::forward_as_tuple(key),
               std::forward_as_tuple(profile_prefs_, key))
      .first->second;
}

void FeedStream::SetContentOrder(const StreamType& stream_type,
                                 ContentOrder content_order) {
  if (!stream_type.IsWebFeed()) {
    DLOG(ERROR) << "SetContentOrder is not supported for this stream_type "
                << stream_type;
    return;
  }

  ContentOrder current_order = GetValidWebFeedContentOrder(*profile_prefs_);
  prefs::SetWebFeedContentOrder(*profile_prefs_, content_order);
  if (current_order == content_order)
    return;

  // Note that ForceRefreshTask clears stored content and forces a network
  // refresh. It is possible to instead cache each ordering of the Feed
  // separately, so that users who switch back and forth can do so more quickly
  // and efficiently. However, there are some reasons to avoid this
  // optimization:
  // * we want content to be fresh, so this optimization would have limited
  //   effect.
  // * interactions with the feed can modify content; in these cases we would
  //   want a full refresh.
  // * it will add quite a bit of complexity to do it right
  task_queue_.AddTask(
      std::make_unique<offline_pages::ClosureTask>(base::BindOnce(
          &FeedStream::ForceRefreshTask, base::Unretained(this), stream_type)));
}

ContentOrder FeedStream::GetContentOrder(const StreamType& stream_type) {
  if (!stream_type.IsWebFeed()) {
    NOTREACHED()
        << "GetContentOrderFromPrefs is not supported for this stream_type "
        << stream_type;
    return ContentOrder::kUnspecified;
  }
  return GetValidWebFeedContentOrder(*profile_prefs_);
}

ContentOrder FeedStream::GetContentOrderFromPrefs(
    const StreamType& stream_type) {
  if (!stream_type.IsWebFeed()) {
    NOTREACHED()
        << "GetContentOrderFromPrefs is not supported for this stream_type "
        << stream_type;
    return ContentOrder::kUnspecified;
  }
  return prefs::GetWebFeedContentOrder(*profile_prefs_);
}

}  // namespace feed
