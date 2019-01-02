// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_utils.h"

#include "content/public/common/browser_side_navigation_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(UrlUtilsTest, IsURLHandledByNetworkStack) {
  EXPECT_TRUE(IsURLHandledByNetworkStack(GURL("http://foo/bar.html")));
  EXPECT_TRUE(IsURLHandledByNetworkStack(GURL("https://foo/bar.html")));
  EXPECT_TRUE(IsURLHandledByNetworkStack(GURL("data://foo")));
  EXPECT_TRUE(IsURLHandledByNetworkStack(GURL("cid:foo@bar")));

  EXPECT_FALSE(IsURLHandledByNetworkStack(GURL("about:blank")));
  EXPECT_FALSE(IsURLHandledByNetworkStack(GURL("about:srcdoc")));
  EXPECT_FALSE(IsURLHandledByNetworkStack(GURL("javascript:foo.js")));
  EXPECT_FALSE(IsURLHandledByNetworkStack(GURL()));
}

TEST(UrlUtilsTest, IsSafeRedirectTarget) {
  EXPECT_FALSE(IsSafeRedirectTarget(GURL(), GURL("chrome://foo/bar.html")));
  EXPECT_TRUE(IsSafeRedirectTarget(GURL(), GURL("http://foo/bar.html")));
  EXPECT_FALSE(IsSafeRedirectTarget(GURL(), GURL("file:///foo/bar/")));
  EXPECT_TRUE(
      IsSafeRedirectTarget(GURL("file:///foo/bar"), GURL("file:///foo/bar/")));
  EXPECT_TRUE(IsSafeRedirectTarget(GURL("filesystem://foo/bar"),
                                   GURL("filesystem://foo/bar/")));
  EXPECT_TRUE(IsSafeRedirectTarget(GURL(), GURL("unknown://foo/bar/")));
  EXPECT_FALSE(IsSafeRedirectTarget(GURL("http://foo/bar.html"),
                                    GURL("file:///foo/bar/")));
  EXPECT_TRUE(IsSafeRedirectTarget(GURL("file:///foo/bar/"),
                                   GURL("http://foo/bar.html")));

  // TODO(cmumford): Capturing current behavior, but should probably prevent
  //                 redirect to invalid URL.
  EXPECT_TRUE(IsSafeRedirectTarget(GURL(), GURL()));
}

}  // namespace content
