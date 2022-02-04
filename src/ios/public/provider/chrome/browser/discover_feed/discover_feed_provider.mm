// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void DiscoverFeedProvider::StartFeed(
    DiscoverFeedConfiguration* discover_config) {}

void DiscoverFeedProvider::StopFeed() {}

bool DiscoverFeedProvider::IsDiscoverFeedEnabled() {
  return false;
}

UIViewController*
DiscoverFeedProvider::NewDiscoverFeedViewControllerWithConfiguration(
    DiscoverFeedViewControllerConfiguration* configuration) {
  return nil;
}

UIViewController*
DiscoverFeedProvider::NewFollowingFeedViewControllerWithConfiguration(
    DiscoverFeedViewControllerConfiguration* configuration) {
  return nil;
}

void DiscoverFeedProvider::RemoveFeedViewController(
    UIViewController* feedViewController) {}

void DiscoverFeedProvider::UpdateTheme() {}

void DiscoverFeedProvider::RefreshFeed() {}

void DiscoverFeedProvider::UpdateFeedForAccountChange() {}

void DiscoverFeedProvider::AddObserver(Observer* observer) {}
void DiscoverFeedProvider::RemoveObserver(Observer* observer) {}
