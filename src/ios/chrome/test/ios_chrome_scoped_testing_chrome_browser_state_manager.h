// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_CHROME_BROWSER_STATE_MANAGER_H_
#define IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_CHROME_BROWSER_STATE_MANAGER_H_

#include <memory>

#include "base/macros.h"

namespace ios {
class ChromeBrowserStateManager;
}

// Helper class to register an ios::ChromeBrowserStateManager against
// the TestApplicationContext using local scoping to do proper cleanup.
class IOSChromeScopedTestingChromeBrowserStateManager {
 public:
  explicit IOSChromeScopedTestingChromeBrowserStateManager(
      std::unique_ptr<ios::ChromeBrowserStateManager> browser_state_manager);
  ~IOSChromeScopedTestingChromeBrowserStateManager();

 private:
  std::unique_ptr<ios::ChromeBrowserStateManager> browser_state_manager_;
  ios::ChromeBrowserStateManager* saved_browser_state_manager_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeScopedTestingChromeBrowserStateManager);
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_CHROME_BROWSER_STATE_MANAGER_H_
