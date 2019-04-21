// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements which rely on focus. */

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_interactive_ui_test.js']);

function CrElementsFocusTest() {}

CrElementsFocusTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),
};

function CrElementsActionMenuTest() {}

CrElementsActionMenuTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.html',

  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../settings/test_util.js',
    'cr_action_menu_test.js',
  ]),
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsActionMenuTest', 'MAYBE_All', function() {
  mocha.run();
});

function CrElementsProfileAvatarSelectorFocusTest() {}

CrElementsProfileAvatarSelectorFocusTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_profile_avatar_selector/' +
      'cr_profile_avatar_selector.html',

  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    'cr_profile_avatar_selector_tests.js',
  ]),
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsProfileAvatarSelectorFocusTest', 'MAYBE_All', function() {
  cr_profile_avatar_selector.registerTests();
  mocha.grep(cr_profile_avatar_selector.TestNames.Focus).run();
});

/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsToggleTest() {}

CrElementsToggleTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_toggle/cr_toggle.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../settings/test_util.js',
    'cr_toggle_test.js',
  ]),
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsToggleTest', 'MAYBE_All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsCheckboxTest() {}

CrElementsCheckboxTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../settings/test_util.js',
    'cr_checkbox_test.js',
  ]),
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsCheckboxTest', 'MAYBE_All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsInputTest() {}

CrElementsInputTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_input/cr_input.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    '../settings/test_util.js',
    'cr_input_test.js',
  ]),
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsInputTest', 'MAYBE_All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsIconButtonFocusTest() {}

CrElementsIconButtonFocusTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    'cr_icon_button_focus_tests.js',
  ]),
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsIconButtonFocusTest', 'MAYBE_All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {CrElementsFocusTest}
 */
function CrElementsExpandButtonTest() {}

CrElementsExpandButtonTest.prototype = {
  __proto__: CrElementsFocusTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.html',

  /** @override */
  extraLibraries: CrElementsFocusTest.prototype.extraLibraries.concat([
    ROOT_PATH + 'ui/webui/resources/js/util.js',
    '../settings/test_util.js',
    'cr_expand_button_focus_tests.js',
  ]),
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('CrElementsExpandButtonTest', 'MAYBE_All', function() {
  mocha.run();
});
