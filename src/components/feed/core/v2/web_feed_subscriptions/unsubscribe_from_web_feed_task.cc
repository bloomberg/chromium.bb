// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/unsubscribe_from_web_feed_task.h"

#include <algorithm>

#include "base/bind.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"

namespace feed {

UnsubscribeFromWebFeedTask::UnsubscribeFromWebFeedTask(
    FeedStream* stream,
    const std::string& web_feed_id,
    base::OnceCallback<void(Result)> callback)
    : stream_(*stream),
      web_feed_name_(web_feed_id),
      callback_(std::move(callback)) {}

UnsubscribeFromWebFeedTask::~UnsubscribeFromWebFeedTask() = default;

void UnsubscribeFromWebFeedTask::Run() {
  if (stream_.ClearAllInProgress()) {
    Done(WebFeedSubscriptionRequestStatus::
             kAbortWebFeedSubscriptionPendingClearAll);
    return;
  }
  WebFeedSubscriptionCoordinator::SubscriptionInfo info =
      stream_.subscriptions().FindSubscriptionInfoById(web_feed_name_);
  if (info.status != WebFeedSubscriptionStatus::kSubscribed) {
    Done(WebFeedSubscriptionRequestStatus::kSuccess);
    return;
  }

  if (stream_.IsOffline()) {
    Done(WebFeedSubscriptionRequestStatus::kFailedOffline);
    return;
  }

  feedwire::webfeed::UnfollowWebFeedRequest request;
  SetConsistencyToken(request, stream_.GetMetadata().consistency_token());
  request.set_name(web_feed_name_);
  stream_.GetNetwork().SendApiRequest<UnfollowWebFeedDiscoverApi>(
      request, stream_.GetSyncSignedInGaia(),
      base::BindOnce(&UnsubscribeFromWebFeedTask::RequestComplete,
                     base::Unretained(this)));
}

void UnsubscribeFromWebFeedTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::UnfollowWebFeedResponse> result) {
  if (!result.response_body) {
    Done(WebFeedSubscriptionRequestStatus::kFailedUnknownError);
    return;
  }

  stream_.SetMetadata(feedstore::MaybeUpdateConsistencyToken(
      stream_.GetMetadata(), result.response_body->consistency_token()));

  result_.unsubscribed_feed_name = web_feed_name_;
  Done(WebFeedSubscriptionRequestStatus::kSuccess);
}

void UnsubscribeFromWebFeedTask::Done(WebFeedSubscriptionRequestStatus status) {
  result_.request_status = status;
  std::move(callback_).Run(std::move(result_));
  TaskComplete();
}

}  // namespace feed
