// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "content/public/test/browser_test.h"

using ServiceWorkerAppTest = extensions::PlatformAppBrowserTest;

IN_PROC_BROWSER_TEST_F(ServiceWorkerAppTest, RegisterAndPostMessage) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/service_worker")) << message_;
}
