// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Path to general chrome browser settings and associated utilities.
const BROWSER_SETTINGS_PATH = '../';

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chromeos/constants/chromeos_features.h"');

// Test fixture for the top-level OS settings UI.
OSSettingsUIBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat(BROWSER_SETTINGS_PATH + 'test_util.js');
  }
};

TEST_F('OSSettingsUIBrowserTest', 'All', () => {
  suite('os-settings-ui', () => {
    let ui;

    suiteSetup(() => {
      testing.Test.disableAnimationsAndTransitions();
      ui = assert(document.querySelector('os-settings-ui'));
      ui.$.drawerTemplate.restamp = true;
    });

    setup(() => {
      ui.$.drawerTemplate.if = false;
      Polymer.dom.flush();
    });

    test('showing menu in toolbar is dependent on narrow mode', () => {
      const toolbar = assert(ui.$$('cr-toolbar'));
      toolbar.narrow = true;
      assertTrue(toolbar.showMenu);

      toolbar.narrow = false;
      assertFalse(toolbar.showMenu);
    });

    test('app drawer', async () => {
      assertEquals(null, ui.$$('cr-drawer os-settings-menu'));
      const drawer = ui.$.drawer;
      assertFalse(drawer.open);

      drawer.openDrawer();
      Polymer.dom.flush();
      await test_util.eventToPromise('cr-drawer-opened', drawer);

      // Validate that dialog is open and menu is shown so it will animate.
      assertTrue(drawer.open);
      assertTrue(!!ui.$$('cr-drawer os-settings-menu'));

      drawer.cancel();
      await test_util.eventToPromise('close', drawer);
      // Drawer is closed, but menu is still stamped so its contents remain
      // visible as the drawer slides out.
      assertTrue(!!ui.$$('cr-drawer os-settings-menu'));
    });

    test('app drawer closes when exiting narrow mode', async () => {
      const drawer = ui.$.drawer;
      const toolbar = ui.$$('cr-toolbar');

      // Mimic narrow mode and open the drawer.
      toolbar.narrow = true;
      drawer.openDrawer();
      Polymer.dom.flush();
      await test_util.eventToPromise('cr-drawer-opened', drawer);

      toolbar.narrow = false;
      Polymer.dom.flush();
      await test_util.eventToPromise('close', drawer);
      assertFalse(drawer.open);
    });

    test('advanced UIs stay in sync', () => {
      const main = ui.$$('os-settings-main');
      const floatingMenu = ui.$$('#left os-settings-menu');
      assertTrue(!!main);
      assertTrue(!!floatingMenu);

      assertFalse(!!ui.$$('cr-drawer os-settings-menu'));
      assertFalse(ui.advancedOpenedInMain_);
      assertFalse(ui.advancedOpenedInMenu_);
      assertFalse(floatingMenu.advancedOpened);
      assertFalse(main.advancedToggleExpanded);

      main.advancedToggleExpanded = true;
      Polymer.dom.flush();

      assertFalse(!!ui.$$('cr-drawer os-settings-menu'));
      assertTrue(ui.advancedOpenedInMain_);
      assertTrue(ui.advancedOpenedInMenu_);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(main.advancedToggleExpanded);

      ui.$.drawerTemplate.if = true;
      Polymer.dom.flush();

      const drawerMenu = ui.$$('cr-drawer os-settings-menu');
      assertTrue(!!drawerMenu);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(drawerMenu.advancedOpened);

      // Collapse 'Advanced' in the menu.
      drawerMenu.$.advancedButton.click();
      Polymer.dom.flush();

      // Collapsing it in the menu should not collapse it in the main area.
      assertFalse(drawerMenu.advancedOpened);
      assertFalse(floatingMenu.advancedOpened);
      assertFalse(ui.advancedOpenedInMenu_);
      assertTrue(main.advancedToggleExpanded);
      assertTrue(ui.advancedOpenedInMain_);

      // Expand both 'Advanced's again.
      drawerMenu.$.advancedButton.click();

      // Collapse 'Advanced' in the main area.
      main.advancedToggleExpanded = false;
      Polymer.dom.flush();

      // Collapsing it in the main area should not collapse it in the menu.
      assertFalse(ui.advancedOpenedInMain_);
      assertTrue(drawerMenu.advancedOpened);
      assertTrue(floatingMenu.advancedOpened);
      assertTrue(ui.advancedOpenedInMenu_);
    });
  });

  mocha.run();
});
