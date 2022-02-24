// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI app settings. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chrome/browser/ui/webui/app_settings/web_app_settings_navigation_throttle.h"');

/* eslint-disable no-var */

class AppSettingsBrowserTest extends PolymerTest {
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }

  testGenPreamble() {
    GEN('WebAppSettingsNavigationThrottle::DisableForTesting();');
  }

  get featureList() {
    return {enabled: ['features::kDesktopPWAsWebAppSettingsPage']};
  }
}

var AppSettingsAppTest = class extends AppSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://app-settings/test_loader.html?module=app_settings/app_test.js&host=webui-test';
  }
};

TEST_F('AppSettingsAppTest', 'All', function() {
  mocha.run();
});
