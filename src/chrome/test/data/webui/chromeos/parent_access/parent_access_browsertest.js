// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Parent Access flow tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_browsertest_base.h"');
GEN('#include "content/public/test/browser_test.h"');


var ParentAccessAppTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_app_test.js';
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_app_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessAppTest', 'TestShowAfterFlow', function() {
  this.runMochaTest(parent_access_app_tests.TestNames.TestShowAfterFlow);
});

var ParentAccessControllerTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_controller_test.js';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_controller_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessControllerTest', 'ParentAccessResultFnCalled', function() {
  this.runMochaTest(
      parent_access_controller_tests.TestNames.ParentAccessResultFnCalled);
});

var ParentAccessUITest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_ui_test.js';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_ui_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessUITest', 'TestIsAllowedRequest', function() {
  this.runMochaTest(parent_access_ui_tests.TestNames.TestIsAllowedRequest);
});

TEST_F('ParentAccessUITest', 'TestShouldReceiveAuthHeader', function() {
  this.runMochaTest(
      parent_access_ui_tests.TestNames.TestShouldReceiveAuthHeader);
});


var ParentAccessUIHandlerTest = class extends testing.Test {
  /** @override */
  get typedefCppFixture() {
    return 'MojoWebUIBrowserTest';
  }

  /** @override */
  get browsePreload() {
    return 'chrome://parent-access/test_loader.html?module=' +
        'chromeos/parent_access/parent_access_ui_handler_test.js';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(parent_access_ui_handler_tests.suiteName, testName);
  }
};

TEST_F('ParentAccessUIHandlerTest', 'TestOnParentAccessResult', function() {
  this.runMochaTest(
      parent_access_ui_handler_tests.TestNames.TestOnParentAccessResult);
});
