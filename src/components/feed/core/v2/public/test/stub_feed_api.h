// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_FEED_API_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_FEED_API_H_

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"
#include "components/feed/core/v2/public/test/stub_web_feed_subscriptions.h"

namespace feed {

class StubPersistentKeyValueStore : public PersistentKeyValueStore {
 public:
  StubPersistentKeyValueStore() = default;
  ~StubPersistentKeyValueStore() override = default;

  void ClearAll(ResultCallback callback) override {}
  void Put(const std::string& key,
           const std::string& value,
           ResultCallback callback) override {}
  void Get(const std::string& key, ResultCallback callback) override {}
  void Delete(const std::string& key, ResultCallback callback) override {}

 private:
};

class StubFeedApi : public FeedApi {
 public:
  WebFeedSubscriptions& subscriptions() override;
  void AttachSurface(FeedStreamSurface*) override {}
  void DetachSurface(FeedStreamSurface*) override {}
  void AddUnreadContentObserver(const StreamType& stream_type,
                                UnreadContentObserver* observer) override {}
  void RemoveUnreadContentObserver(const StreamType& stream_type,
                                   UnreadContentObserver* observer) override {}
  bool IsArticlesListVisible() override;
  bool IsActivityLoggingEnabled(const StreamType& stream_type) const override;
  std::string GetClientInstanceId() const override;
  std::string GetSessionId() const override;
  void ExecuteRefreshTask(RefreshTaskId task_id) override {}
  void LoadMore(const FeedStreamSurface& surface,
                base::OnceCallback<void(bool)> callback) override {}
  void ManualRefresh(const StreamType& stream_type,
                     base::OnceCallback<void(bool)> callback) override {}
  ImageFetchId FetchImage(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) override;
  void CancelImageFetch(ImageFetchId id) override {}
  PersistentKeyValueStore& GetPersistentKeyValueStore() override;
  void ExecuteOperations(
      const StreamType& stream_type,
      std::vector<feedstore::DataOperation> operations) override {}
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
  void ProcessThereAndBackAgain(base::StringPiece data) override {}
  void ProcessThereAndBackAgain(
      base::StringPiece data,
      const feedui::LoggingParameters& logging_parameters) override {}
  void ProcessViewAction(base::StringPiece data) override {}
  void ProcessViewAction(
      base::StringPiece data,
      const feedui::LoggingParameters& logging_parameters) override {}
  bool WasUrlRecentlyNavigatedFromFeed(const GURL& url) override;
  void ReportSliceViewed(SurfaceId surface_id,
                         const StreamType& stream_type,
                         const std::string& slice_id) override {}
  void ReportFeedViewed(const StreamType& stream_type,
                        SurfaceId surface_id) override {}
  void ReportPageLoaded() override {}
  void ReportOpenAction(const GURL& url,
                        const StreamType& stream_type,
                        const std::string& slice_id) override {}
  void ReportOpenVisitComplete(base::TimeDelta visit_time) override {}
  void ReportOpenInNewTabAction(const GURL& url,
                                const StreamType& stream_type,
                                const std::string& slice_id) override {}
  void ReportStreamScrolled(const StreamType& stream_type,
                            int distance_dp) override {}
  void ReportStreamScrollStart() override {}
  void ReportOtherUserAction(const StreamType& stream_type,
                             FeedUserActionType action_type) override {}
  void ReportNoticeCreated(const StreamType& stream_type,
                           const std::string& key) override {}
  void ReportNoticeViewed(const StreamType& stream_type,
                          const std::string& key) override {}
  void ReportNoticeOpenAction(const StreamType& stream_type,
                              const std::string& key) override {}
  void ReportNoticeDismissed(const StreamType& stream_type,
                             const std::string& key) override {}
  DebugStreamData GetDebugStreamData() override;
  void ForceRefreshForDebugging(const StreamType& stream_type) override {}
  std::string DumpStateForDebugging() override;
  void SetForcedStreamUpdateForDebugging(
      const feedui::StreamUpdate& stream_update) override {}
  base::Time GetLastFetchTime(const StreamType& stream_type) override;
  void SetContentOrder(const StreamType& stream_type,
                       ContentOrder content_order) override {}
  ContentOrder GetContentOrder(const StreamType& stream_type) override;
  ContentOrder GetContentOrderFromPrefs(const StreamType& stream_type) override;

 private:
  StubWebFeedSubscriptions web_feed_subscriptions_;
  StubPersistentKeyValueStore persistent_key_value_store_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_FEED_API_H_
