// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_INSERTER_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_INSERTER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class NavigationThrottle;
class WebContents;

using ThrottleInsertionCallback =
    base::RepeatingCallback<std::unique_ptr<NavigationThrottle>(
        NavigationHandle*)>;

// This class is instantiated with a NavigationThrottle factory callback, and
//  - Calls the callback in every DidStartNavigation.
//  - If the throttle is successfully created, registers it with the given
//    navigation.
class TestNavigationThrottleInserter : public WebContentsObserver {
 public:
  TestNavigationThrottleInserter(WebContents* web_contents,
                                 ThrottleInsertionCallback callback);
  ~TestNavigationThrottleInserter() override;

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;

 private:
  ThrottleInsertionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationThrottleInserter);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_INSERTER_H_
