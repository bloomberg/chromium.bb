// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_ENUMS_H_

#include <iosfwd>

namespace feed {

// One value for each network API method used by the feed.
enum class NetworkRequestType : int {
  kFeedQuery = 0,
  kUploadActions = 1,
  kNextPage = 2,
  kListWebFeeds = 3,
  kUnfollowWebFeed = 4,
  kFollowWebFeed = 5,
  kListRecommendedWebFeeds = 6,
  kWebFeedListContents = 7,
  kQueryInteractiveFeed = 8,
  kQueryBackgroundFeed = 9,
  kQueryNextPage = 10,
};

// Denotes how the stream content loading is used for.
enum class LoadType {
  // Loads the stream model into memory. If successful, this directly forces a
  // model load in |FeedStream()| before completing the task.
  kInitialLoad = 0,
  // Loads additional content from the network when the model is already loaded.
  kLoadMore = 1,
  // Refreshes the stored stream data from the network, on the background. This
  // will fail if the model is already loaded.
  kBackgroundRefresh = 2,
  // Refreshes the stored stream data from the network, per the user request.
  // The stored stream data and the loaded model will not be affected if the
  // network request fails.
  kManualRefresh = 3,
};

// This must be kept in sync with FeedLoadStreamStatus in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.v2
enum class LoadStreamStatus {
  // Loading was not attempted.
  kNoStatus = 0,

  // Final loading statuses where loading succeeds. :
  kLoadedFromStore = 1,
  kLoadedFromNetwork = 2,
  kLoadedStaleDataFromStoreDueToNetworkFailure = 21,

  // Statuses where data is loaded from the persistent store, but is stale.
  kDataInStoreIsStale = 8,
  // The timestamp for stored data is in the future, so we're treating stored
  // data as it it is stale.
  kDataInStoreIsStaleTimestampInFuture = 9,
  kDataInStoreStaleMissedLastRefresh = 20,

  // Failure statuses where content is not loaded.
  kFailedWithStoreError = 3,
  kNoStreamDataInStore = 4,
  kModelAlreadyLoaded = 5,
  kNoResponseBody = 6,
  kProtoTranslationFailed = 7,
  kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED = 10,
  kCannotLoadFromNetworkOffline = 11,
  kCannotLoadFromNetworkThrottled = 12,
  kLoadNotAllowedEulaNotAccepted = 13,
  kLoadNotAllowedArticlesListHidden = 14,
  kCannotParseNetworkResponseBody = 15,
  kLoadMoreModelIsNotLoaded = 16,
  kLoadNotAllowedDisabledByEnterprisePolicy = 17,
  kNetworkFetchFailed = 18,
  kCannotLoadMoreNoNextPageToken = 19,
  kDataInStoreIsExpired = 22,
  kDataInStoreIsForAnotherUser = 23,
  kAbortWithPendingClearAll = 24,
  kAlreadyHaveUnreadContent = 25,
  kNotAWebFeedSubscriber = 26,
  kMaxValue = kNotAWebFeedSubscriber,
};

// Were we able to load fresh Feed data. This should be 'true' unless some kind
// of error occurred.
bool IsLoadingSuccessfulAndFresh(LoadStreamStatus status);

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value);

// Keep this in sync with FeedUploadActionsStatus in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.v2
enum class UploadActionsStatus {
  kNoStatus = 0,
  kNoPendingActions = 1,
  kFailedToStorePendingAction = 2,
  kStoredPendingAction = 3,
  kUpdatedConsistencyToken = 4,
  kFinishedWithoutUpdatingConsistencyToken = 5,
  kAbortUploadForSignedOutUser = 6,
  kAbortUploadBecauseDisabled = 7,
  kAbortUploadForWrongUser = 8,
  kAbortUploadActionsWithPendingClearAll = 9,
  kMaxValue = kAbortUploadActionsWithPendingClearAll,
};

// Keep this in sync with FeedUploadActionsBatchStatus in enums.xml.
enum class UploadActionsBatchStatus {
  kNoStatus = 0,
  kFailedToUpdateStore = 1,
  kFailedToUpload = 2,
  kFailedToRemoveUploadedActions = 3,
  kExhaustedUploadQuota = 4,
  kAllActionsWereStale = 5,
  kSuccessfullyUploadedBatch = 6,
  kMaxValue = kSuccessfullyUploadedBatch,
};

std::ostream& operator<<(std::ostream& out, UploadActionsStatus value);
std::ostream& operator<<(std::ostream& out, UploadActionsBatchStatus value);

// This must be kept in sync with WebFeedRefreshStatus in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Status of updating recommended or subscribed web feeds.
enum class WebFeedRefreshStatus {
  kNoStatus = 0,
  kSuccess = 1,
  kNetworkFailure = 2,
  kNetworkRequestThrottled = 3,
  kAbortFetchWebFeedPendingClearAll = 4,
  kMaxValue = kAbortFetchWebFeedPendingClearAll,
};
std::ostream& operator<<(std::ostream& out, WebFeedRefreshStatus value);

// Tells how the notice acknowledgement is reached. These values are also used
// in histograms. Entries should not be renumbered and numeric values should
// never be reused.
enum class NoticeAcknowledgementPath {
  // The acknowledgment is reached after the user views the notice for a
  // required number of times (currently it is 3).
  kViaViewing = 0,
  // The acknowledgment is reached after the user taps the notice to perform
  // an open action for a required number of times (currently it is 1).
  kViaOpenAction = 1,
  // The acknowledgement is reached after the user taps X button to close
  // the notice.
  kViaDismissal = 2,
  kMaxValue = kViaDismissal,
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_ENUMS_H_
