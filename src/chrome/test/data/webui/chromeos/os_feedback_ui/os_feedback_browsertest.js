// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test fixture for chrome://os-feedback.
 * Unifieid polymer testing suite for feedback tool.
 *
 * To run all tests in a single instance (default, faster):
 * `browser_tests --gtest_filter=OSFeedback*``
 *
 * To run each test in a new instance:
 * `browser_tests --run-manual --gtest_filter=OSFeedback.MANUAL_*``
 *
 * To run a single test suite, such as 'ConfirmationPageTest':
 * `browser_tests --run-manual \
 *  --gtest_filter=OSFeedback.MANUAL_ConfirmationPageTest`
 *
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

this.OSFeedbackBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-feedback/test_loader.html?module=chromeos/' +
        'os_feedback_ui/os_feedback_unified_test.js';
  }

  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kOsFeedback']};
  }
};

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'confirmationPageTest',
  'fakeMojoProviderTest',
  'searchPageTest',
];

TEST_F('OSFeedbackBrowserTest', 'All', function() {
  assertDeepEquals(
      debug_suites_list, test_suites_list,
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F('OSFeedbackBrowserTest', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });
}
