// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_PLATFORM_PART_H_
#define CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_PLATFORM_PART_H_

#include "base/macros.h"
#include "chrome/browser/browser_process_platform_part.h"

// A TestingBrowserProcessPlatformPart is essentially a
// BrowserProcessPlatformPart except it doesn't have an OomPriorityManager on
// Chrome OS.
class TestingBrowserProcessPlatformPart : public BrowserProcessPlatformPart {
 public:
  TestingBrowserProcessPlatformPart();
  ~TestingBrowserProcessPlatformPart() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcessPlatformPart);
};

#endif  // CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_PLATFORM_PART_H_
