// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI read later. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/* eslint-disable no-var */

class SidePanelBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

var SidePanelAppTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/side_panel_app_test.js&host=webui-test';
  }
};

TEST_F('SidePanelAppTest', 'All', function() {
  mocha.run();
});

var SidePanelBookmarksListTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmarks_list_test.js&host=webui-test';
  }
};

TEST_F('SidePanelBookmarksListTest', 'All', function() {
  mocha.run();
});


var SidePanelBookmarkFolderTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmark_folder_test.js&host=webui-test';
  }
};

TEST_F('SidePanelBookmarkFolderTest', 'All', function() {
  mocha.run();
});


var SidePanelBookmarksDragManagerTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/bookmarks/bookmarks_drag_manager_test.js&host=webui-test';
  }
};

TEST_F('SidePanelBookmarksDragManagerTest', 'All', function() {
  mocha.run();
});

var ReadingListAppTest = class extends SidePanelBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=side_panel/reading_list/reading_list_app_test.js&host=webui-test';
  }
};

TEST_F('ReadingListAppTest', 'All', function() {
  mocha.run();
});
