// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Accessibility Settings tests. */

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  '//chrome/test/data/webui/a11y/accessibility_test.js',
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  '//chrome/test/data/webui/settings/a11y/settings_accessibility_test.js',
]);

GEN('#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"');
GEN('#include "chrome/browser/profiles/profile.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/prefs/pref_service.h"');

/**
 * Test fixture for Accessibility of Chrome Settings.
 * @constructor
 * @extends {SettingsAccessibilityTest}
 */
function CrostiniAccessibilityTest() {}

CrostiniAccessibilityTest.prototype = {
  __proto__: SettingsAccessibilityTest.prototype,

  featureList: {enabled: ['features::kCrostini']},

  testGenPreamble: function() {
    GEN('  browser()->profile()->GetPrefs()->SetBoolean(');
    GEN('      crostini::prefs::kCrostiniEnabled, true);');
  },
};
