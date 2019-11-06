// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_TEST_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_TEST_UTIL_H_

#include "chrome/browser/extensions/extension_service_test_base.h"

// Base class for app list unit tests that use the "app_list" test profile.
class AppListTestBase : public extensions::ExtensionServiceTestBase {
 public:
  static const char kHostedAppId[];
  static const char kPackagedApp1Id[];
  static const char kPackagedApp2Id[];

  AppListTestBase();
  ~AppListTestBase() override;

  void SetUp() override;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_TEST_UTIL_H_
