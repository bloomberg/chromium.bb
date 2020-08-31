// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://help-app.
 */

GEN('#include "chromeos/components/help_app_ui/test/help_app_ui_browsertest.h"');

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://help-app';

// Test driver initialised in setUp and used in tests to interact with the
// untrusted context.
let driver = null;

var HelpAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//chromeos/components/help_app_ui/test/driver.js',
      '//ui/webui/resources/js/assert.js',
    ];
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kHelpAppV2']};
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get typedefCppFixture() {
    return 'HelpAppUiBrowserTest';
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  setUp() {
    super.setUp();
    driver = new GuestDriver(GUEST_ORIGIN);
  }

  /** @override */
  tearDown() {
    driver.tearDown();
    super.tearDown();
  }
};

// Tests that chrome://help-app goes somewhere instead of 404ing or crashing.
TEST_F('HelpAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  const guest = document.querySelector('iframe');

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + '/app.html');
  testDone();
});

// Tests that we have localised information in the HTML like title and lang.
TEST_F('HelpAppUIBrowserTest', 'HasTitleAndLang', () => {
  assertEquals(document.documentElement.lang, 'en');
  assertEquals(document.title, 'Explore');
  testDone();
});

// Tests that trusted context can successfully send a request to open the
// feedback dialog and receive a response.
TEST_F('HelpAppUIBrowserTest', 'CanOpenFeedbackDialog', async () => {
  const result = await help_app.handler.openFeedbackDialog();

  assertEquals(result.errorMessage, '');
  testDone();
});

// Tests that untrusted context can successfully send a request to open the
// feedback dialog and receive a response.
TEST_F('HelpAppUIBrowserTest', 'GuestCanOpenFeedbackDialog', async () => {
  const result = await driver.sendPostMessageRequest('feedback');

  // No error message from opening feedback dialog.
  assertEquals(result.errorMessage, '');
  testDone();
});

// Test cases injected into the guest context.
// See implementations in help_app_guest_ui_browsertest.js.

TEST_F('HelpAppUIBrowserTest', 'GuestHasLang', async () => {
  await driver.runTestInGuest('GuestHasLang');
  testDone();
});
