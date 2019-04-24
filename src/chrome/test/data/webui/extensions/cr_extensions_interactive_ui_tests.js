// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Extensions interactive UI tests. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_interactive_ui_test.js']);
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

/**
 * Test fixture for interactive Polymer Extensions elements.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
const CrExtensionsInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/';
  }

  /** @override */
  get extraLibraries() {
    return PolymerTest.getLibraries(ROOT_PATH).concat([
      '../settings/test_util.js',
    ]);
  }
};


/** Test fixture for Sync Page. */
CrExtensionsOptionsPageTest = class extends CrExtensionsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/?id=ibbpngabdmdpednkhonkkobdeccpkiff';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_options_dialog_test.js',
    ]);
  }

  /** @override */
  testGenPreamble() {
    GEN('  InstallExtensionWithInPageOptions();');
  }

  /** @override */
  get typedefCppFixture() {
    return 'ExtensionSettingsUIBrowserTest';
  }
};

TEST_F('CrExtensionsOptionsPageTest', 'All', function() {
  mocha.run();
});
