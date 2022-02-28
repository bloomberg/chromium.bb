// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_

#include "base/compiler_specific.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/net_errors.h"

class GURL;

namespace content {

class RenderFrameHost;

namespace test {

// Browser tests can use this class to more conveniently leverage fenced frames.
// Note that this only applies to the MPArch version of fenced frames, and not
// fenced frames based on the ShadowDOM architecture.
class FencedFrameTestHelper {
 public:
  FencedFrameTestHelper();
  ~FencedFrameTestHelper();
  FencedFrameTestHelper(const FencedFrameTestHelper&) = delete;
  FencedFrameTestHelper& operator=(const FencedFrameTestHelper&) = delete;

  // This method creates a new fenced frame rooted at `fenced_frame_parent` that
  // is navigated to `url`. This method waits for the navigation to `url` to
  // commit, and returns the `RenderFrameHost` that committed the navigation if
  // it succeeded. Otherwise, it returns `nullptr`. See
  // `NavigationFrameInFencedFrameTree()` documentation for the
  // `expected_error_code` parameter.
  RenderFrameHost* CreateFencedFrame(RenderFrameHost* fenced_frame_parent,
                                     const GURL& url,
                                     net::Error expected_error_code = net::OK);

  // This method provides a way to navigate frames within a fenced frame's tree,
  // and synchronously wait for the load to finish. The reason we have this
  // method is because navigations inside of a fenced frame's tree cannot be
  // synchronously waited on via the traditional means of using e.g.,
  // `TestFrameNavigationObserver`. This method returns the `RenderFrameHost`
  // that the navigation committed to if it was successful (which may be
  // different from the one that navigation started in), and `nullptr`
  // otherwise. It takes an `expected_error_code` in case the navigation to
  // `url` fails, which can be detected on a per-error-code basis.
  // TODO(crbug.com/1199682): Fix the underlying reason why we cannot use
  // traditional means of waiting for a navigation to finish loading inside a
  // fenced frame tree.
  RenderFrameHost* NavigateFrameInFencedFrameTree(
      RenderFrameHost* rfh,
      const GURL& url,
      net::Error expected_error_code = net::OK);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace test

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_
