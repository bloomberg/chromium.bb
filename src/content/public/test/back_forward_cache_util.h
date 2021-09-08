// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_
#define CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "content/public/browser/back_forward_cache.h"

namespace content {
class WebContents;

// This is a helper class to check in the tests that back-forward cache
// was disabled for a particular reason.
//
// This class should be created in the beginning of the test and will
// know about all BackForwardCache::DisableForRenderFrameHost which
// happened during its lifetime.
//
// Typical usage pattern:
//
// BackForwardCacheDisabledTester helper;
// NavigateToURL(page_with_feature);
// NavigateToURL(away);
// EXPECT_TRUE/FALSE(helper.IsDisabledForFrameWithReason());

class BackForwardCacheDisabledTester {
 public:
  BackForwardCacheDisabledTester();
  ~BackForwardCacheDisabledTester();

  bool IsDisabledForFrameWithReason(int process_id,
                                    int frame_routing_id,
                                    BackForwardCache::DisabledReason reason);

 private:
  // Impl has to inherit from BackForwardCacheImpl, which is
  // a content/-internal concept, so we can include it only from
  // .cc file.
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Helper function to be used when the tests are interested in covering the
// scenarios when back-forward cache is not used. This is similar to method
// BackForwardCache::DisableForTesting(), but it takes a WebContents instead of
// a BackForwardCache. This method disables BackForwardCache for a given
// WebContents with the reason specified.
//
// Note that it is preferred to make the test work with BackForwardCache when
// feasible, or have a standalone test with BackForwardCache enabled to test
// the functionality when necessary.
void DisableBackForwardCacheForTesting(
    WebContents* web_contents,
    BackForwardCache::DisableForTestingReason reason);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_
