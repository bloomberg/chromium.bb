// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for History which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

const HistoryFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {enabled: ['network::features::kOutOfBlinkCors']};
  }
};

// eslint-disable-next-line no-var
var HistoryToolbarFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_toolbar_focus_test.js';
  }
};

TEST_F('HistoryToolbarFocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryListFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_list_focus_test.js';
  }
};

GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_AllListFocus DISABLED_All');
GEN('#else');
GEN('#define MAYBE_AllListFocus All');
GEN('#endif');
TEST_F('HistoryListFocusTest', 'MAYBE_AllListFocus', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistorySyncedDeviceManagerFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_synced_device_manager_focus_test.js';
  }
};

TEST_F('HistorySyncedDeviceManagerFocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryItemFocusTest = class extends HistoryFocusTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_item_focus_test.js';
  }
};

TEST_F('HistoryItemFocusTest', 'All', function() {
  mocha.run();
});
