// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  '//chrome/test/data/webui/a11y/accessibility_test.js',
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);
GEN('#include "chrome/browser/ui/webui/welcome/nux_helper.h"');

OnboardingA11y = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['nux::kNuxOnboardingForceEnabled']};
  }
};

AccessibilityTest.define('OnboardingA11y', {
  // Must be unique within the test fixture and cannot have spaces.
  name: 'OnboardingFlow',

  // Optional. Configuration for axe-core. Can be used to disable a test.
  axeOptions: {},

  // Optional. Filter on failures. Use this for individual false positives.
  violationFilter: {},

  // Optional. Any setup required for all tests. This will run before each one.
  setup: function() {},

  tests: {
    'Landing Page': function() {
      // Make sure we're in the right page.
      assertEquals('Make Chrome your own', getDeepActiveElement().textContent);
    },
  },
});
