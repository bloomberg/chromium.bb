// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Crostini Upgrader page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

function CrostiniUpgraderBrowserTest() {}

CrostiniUpgraderBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload:
      'chrome://crostini-upgrader/test_loader.html?module=chromeos/crostini_upgrader_app_test.js',

  featureList: {enabled: ['chromeos::features::kCrostiniWebUIUpgrader']},
};


TEST_F('CrostiniUpgraderBrowserTest', 'All', function() {
  mocha.run();
});
