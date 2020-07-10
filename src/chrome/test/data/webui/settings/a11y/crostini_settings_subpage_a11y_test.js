// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the CROSTINI route.
 * Chrome OS only.
 */

GEN_INCLUDE([
  'crostini_accessibility_test.js',
]);

AccessibilityTest.define('CrostiniAccessibilityTest', {
  /** @override */
  name: 'CROSTINI',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptionsExcludeLinkInTextBlock,
  /** @override */
  setup: function() {
    settings.router.navigateTo(settings.routes.CROSTINI);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});
