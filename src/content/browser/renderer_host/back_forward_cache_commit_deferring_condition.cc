// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_commit_deferring_condition.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"

namespace content {

// static
std::unique_ptr<CommitDeferringCondition>
BackForwardCacheCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  if (!navigation_request.IsServedFromBackForwardCache())
    return nullptr;

  // Currently only navigations in the primary main frame can restore pages
  // from BFCache.
  DCHECK(navigation_request.IsInPrimaryMainFrame());

  return base::WrapUnique(
      new BackForwardCacheCommitDeferringCondition(navigation_request));
}

BackForwardCacheCommitDeferringCondition::
    BackForwardCacheCommitDeferringCondition(
        NavigationRequest& navigation_request)
    : navigation_request_(navigation_request) {}

BackForwardCacheCommitDeferringCondition::
    ~BackForwardCacheCommitDeferringCondition() = default;

CommitDeferringCondition::Result
BackForwardCacheCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  DCHECK(navigation_request_.IsServedFromBackForwardCache());

  BackForwardCacheImpl& bfcache = navigation_request_.frame_tree_node()
                                      ->navigator()
                                      .controller()
                                      .GetBackForwardCache();

  // If an entry doesn't exist (it was evicted?) there's no need to defer the
  // commit as we'll end up performing a new navigation.
  BackForwardCacheImpl::Entry* bfcache_entry =
      bfcache.GetEntry(navigation_request_.nav_entry_id());
  if (!bfcache_entry)
    return Result::kProceed;

  bfcache.WillCommitNavigationToCachedEntry(*bfcache_entry, std::move(resume));
  return Result::kDefer;
}

}  // namespace content
