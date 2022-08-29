// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_WEB_FEED_UTIL_H_
#define CHROME_BROWSER_FEED_WEB_FEED_UTIL_H_

#include <string>
#include "base/callback.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"

class Profile;
namespace content {
class WebContents;
}

namespace feed {

class WebFeedSubscriptions;

// Defines common functionalities that can be used by mobile version and
// desktop version.

WebFeedSubscriptions* GetSubscriptionsForProfile(Profile* profile);

void FindWebFeedInfoForPage(content::WebContents* web_contents,
                            WebFeedPageInformationRequestReason reason,
                            base::OnceCallback<void(WebFeedMetadata)> callback);

void FollowWebFeed(
    content::WebContents* web_contents,
    base::OnceCallback<void(WebFeedSubscriptions::FollowWebFeedResult)>
        callback);

void UnfollowWebFeed(
    const std::string& web_feed_id,
    bool is_durable_request,
    base::OnceCallback<void(WebFeedSubscriptions::UnfollowWebFeedResult)>
        callback);

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_WEB_FEED_UTIL_H_
