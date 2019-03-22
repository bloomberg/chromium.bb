// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace web_app {

class TestWebUIControllerFactory;

class SystemWebAppManagerBrowserTest : public InProcessBrowserTest {
 public:
  SystemWebAppManagerBrowserTest();
  ~SystemWebAppManagerBrowserTest() override;

  // Overridden from InProcessBrowserTest:
  void SetUpOnMainThread() override;

 protected:
  Browser* InstallAndLaunchSystemApp();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestWebUIControllerFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerBrowserTest);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
