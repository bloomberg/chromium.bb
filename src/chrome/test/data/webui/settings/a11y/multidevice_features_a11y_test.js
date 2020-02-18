// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MULTIDEVICE_FEATURES route.
 * Chrome OS only.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'settings_accessibility_test.js',
]);
GEN('#include "chromeos/constants/chromeos_features.h"');

// eslint-disable-next-line no-var
var MultideviceFeaturesA11yTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    // Always test with SplitSettings on because the pages are the same in the
    // legacy combined settings and we don't want to test everything twice.
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }
};

AccessibilityTest.define('MultideviceFeaturesA11yTest', {
  /** @override */
  name: 'MULTIDEVICE_FEATURES_ACCESSIBILITY',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptionsExcludeLinkInTextBlock,
  /** @override */
  setup: function() {
    settings.router.navigateTo(settings.routes.MULTIDEVICE_FEATURES);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter:
      Object.assign({}, SettingsAccessibilityTest.violationFilter, {
        // Excuse link without an underline.
        // TODO(https://crbug.com/894602): Remove this exception when settled
        // with UX.
        'link-in-text-block': function(nodeResult) {
          return nodeResult.element.parentElement.id == 'multideviceSubLabel';
        },
      }),
});
