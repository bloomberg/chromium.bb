// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/subscribe_to_web_feed_task.h"

#include <algorithm>

#include "base/bind.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/core/v2/web_feed_subscriptions/wire_to_store.h"

namespace feed {

SubscribeToWebFeedTask::SubscribeToWebFeedTask(
    FeedStream* stream,
    Request request,
    base::OnceCallback<void(Result)> callback)
    : stream_(*stream),
      request_(std::move(request)),
      callback_(std::move(callback)) {}

SubscribeToWebFeedTask::~SubscribeToWebFeedTask() = default;

void SubscribeToWebFeedTask::Run() {
  if (stream_.ClearAllInProgress()) {
    Done(WebFeedSubscriptionRequestStatus::
             kAbortWebFeedSubscriptionPendingClearAll);
    return;
  }
  if (!request_.web_feed_id.empty()) {
    DCHECK(request_.page_info.url().is_empty());
    WebFeedSubscriptionCoordinator::SubscriptionInfo info =
        stream_.subscriptions().FindSubscriptionInfoById(request_.web_feed_id);
    if (info.status == WebFeedSubscriptionStatus::kSubscribed) {
      subscribed_web_feed_info_ = info.web_feed_info;
      Done(WebFeedSubscriptionRequestStatus::kSuccess);
      return;
    }
    if (stream_.IsOffline()) {
      Done(WebFeedSubscriptionRequestStatus::kFailedOffline);
      return;
    }
    feedwire::webfeed::FollowWebFeedRequest request;
    SetConsistencyToken(request, stream_.GetMetadata().consistency_token());
    request.set_name(request_.web_feed_id);
    stream_.GetNetwork().SendApiRequest<FollowWebFeedDiscoverApi>(
        request, stream_.GetSyncSignedInGaia(),
        base::BindOnce(&SubscribeToWebFeedTask::RequestComplete,
                       base::Unretained(this)));
  } else {
    DCHECK(request_.page_info.url().is_valid());
    WebFeedSubscriptionCoordinator::SubscriptionInfo info =
        stream_.subscriptions().FindSubscriptionInfo(request_.page_info);
    if (info.status == WebFeedSubscriptionStatus::kSubscribed) {
      subscribed_web_feed_info_ = info.web_feed_info;
      Done(WebFeedSubscriptionRequestStatus::kSuccess);
      return;
    }
    if (stream_.IsOffline()) {
      Done(WebFeedSubscriptionRequestStatus::kFailedOffline);
      return;
    }
    feedwire::webfeed::FollowWebFeedRequest request;
    SetConsistencyToken(request, stream_.GetMetadata().consistency_token());
    request.set_web_page_uri(request_.page_info.url().spec());
    for (const GURL& rss_url : request_.page_info.GetRssUrls()) {
      request.add_page_rss_uris(rss_url.spec());
    }
    stream_.GetNetwork().SendApiRequest<FollowWebFeedDiscoverApi>(
        request, stream_.GetSyncSignedInGaia(),
        base::BindOnce(&SubscribeToWebFeedTask::RequestComplete,
                       base::Unretained(this)));
  }
}

void SubscribeToWebFeedTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::FollowWebFeedResponse> result) {
  if (result.response_body) {
    stream_.SetMetadata(feedstore::MaybeUpdateConsistencyToken(
        stream_.GetMetadata(), result.response_body->consistency_token()));
    subscribed_web_feed_info_ =
        ConvertToStore(*result.response_body->mutable_web_feed());
    Done(WebFeedSubscriptionRequestStatus::kSuccess);
    return;
  }
  // TODO(crbug/1152592): Check for 'too many subscriptions' error.
  Done(WebFeedSubscriptionRequestStatus::kFailedUnknownError);
}

void SubscribeToWebFeedTask::Done(WebFeedSubscriptionRequestStatus status) {
  Result result;
  result.request_status = status;
  result.web_feed_info = subscribed_web_feed_info_;
  result.followed_web_feed_id = subscribed_web_feed_info_.web_feed_id();
  std::move(callback_).Run(std::move(result));
  TaskComplete();
}

}  // namespace feed
