// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

class NewTabPageBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
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
}

// eslint-disable-next-line no-var
var NewTabPageAppTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/app_test.js';
  }
};

TEST_F('NewTabPageAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageMostVisitedTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/most_visited_test.js';
  }
};

TEST_F('NewTabPageMostVisitedTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_dialog_test.js';
  }
};

TEST_F('NewTabPageCustomizeDialogTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeThemesTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_themes_test.js';
  }
};

TEST_F('NewTabPageCustomizeThemesTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageThemeIconTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/theme_icon_test.js';
  }
};

TEST_F('NewTabPageThemeIconTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageUtilsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/utils_test.js';
  }
};

TEST_F('NewTabPageUtilsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeShortcutsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_shortcuts_test.js';
  }
};

TEST_F('NewTabPageCustomizeShortcutsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeBackgroundsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_backgrounds_test.js';
  }
};

TEST_F('NewTabPageCustomizeBackgroundsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageVoiceSearchOverlayTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/voice_search_overlay_test.js';
  }
};

TEST_F('NewTabPageVoiceSearchOverlayTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageFakeboxTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/fakebox_test.js';
  }
};

TEST_F('NewTabPageFakeboxTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageRealboxTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/realbox_test.js';
  }
};

TEST_F('NewTabPageRealboxTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageLogoTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/logo_test.js';
  }
};

TEST_F('NewTabPageLogoTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageDoodleShareDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/doodle_share_dialog_test.js';
  }
};

TEST_F('NewTabPageDoodleShareDialogTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageBackgroundManagerTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/background_manager_test.js';
  }
};

TEST_F('NewTabPageBackgroundManagerTest', 'All', function() {
  mocha.run();
});
