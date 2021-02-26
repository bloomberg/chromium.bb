// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import {BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageCustomizeShortcutsTest', () => {
  /** @type {!CustomizeShortcutsElement} */
  let customizeShortcuts;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    testProxy.handler.setResultFor('addMostVisitedTile', Promise.resolve({
      success: true,
    }));
    testProxy.handler.setResultFor('updateMostVisitedTile', Promise.resolve({
      success: true,
    }));
    BrowserProxy.instance_ = testProxy;

    customizeShortcuts = document.createElement('ntp-customize-shortcuts');
    document.body.appendChild(customizeShortcuts);
  });

  /**
   * @param {boolean} customLinksEnabled
   * @param {boolean} visible
   * @return {!Promise}
   * @private
   */
  async function setInitialSettings(customLinksEnabled, visible) {
    testProxy.callbackRouterRemote.setMostVisitedInfo({
      customLinksEnabled: customLinksEnabled,
      tiles: [],
      visible: visible,
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
  }

  /**
   * @param {boolean} selected
   * @param {!HTMLElement} el
   * @private
   */
  function assertIsSelected(selected, el) {
    assertEquals(selected, el.classList.contains('selected'));
  }

  /**
   * @param {boolean} customLinksEnabled
   * @param {boolean} useMostVisited
   * @param {boolean} hidden
   * @private
   */
  function assertSelection(customLinksEnabled, useMostVisited, hidden) {
    assertEquals(1, customLinksEnabled + useMostVisited + hidden);
    assertIsSelected(
        customLinksEnabled, customizeShortcuts.$.optionCustomLinks);
    assertIsSelected(useMostVisited, customizeShortcuts.$.optionMostVisited);
    assertIsSelected(hidden, customizeShortcuts.$.hide);
    assertEquals(hidden, customizeShortcuts.$.hideToggle.checked);
  }

  /** @private */
  function assertCustomLinksEnabled() {
    assertSelection(
        /* customLinksEnabled= */ true, /* useMostVisited= */ false,
        /* hidden= */ false);
  }

  /** @private */
  function assertUseMostVisited() {
    assertSelection(
        /* customLinksEnabled= */ false, /* useMostVisited= */ true,
        /* hidden= */ false);
  }

  /** @private */
  function assertHidden() {
    assertSelection(
        /* customLinksEnabled= */ false, /* useMostVisited= */ false,
        /* hidden= */ true);
  }

  test('selections are mutually exclusive', () => {
    assertIsSelected(false, customizeShortcuts.$.optionCustomLinks);
    customizeShortcuts.$.optionCustomLinksButton.click();
    assertCustomLinksEnabled();
    customizeShortcuts.$.optionMostVisitedButton.click();
    assertUseMostVisited();
    customizeShortcuts.$.hideToggle.click();
    assertHidden();
    customizeShortcuts.$.hideToggle.click();
    assertUseMostVisited();
  });

  test('enable custom links calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false,
        /* visible= */ false);
    assertHidden();
    customizeShortcuts.$.optionCustomLinksButton.click();
    const setSettingsCalled =
        testProxy.handler.whenCalled('setMostVisitedSettings');
    customizeShortcuts.apply();
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertTrue(customLinksEnabled);
    assertTrue(visible);
  });

  test('use most-visited calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true,
        /* visible= */ false);
    assertHidden();
    customizeShortcuts.$.optionMostVisitedButton.click();
    const setSettingsCalled =
        testProxy.handler.whenCalled('setMostVisitedSettings');
    customizeShortcuts.apply();
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertFalse(customLinksEnabled);
    assertTrue(visible);
  });

  test('toggle hide calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true,
        /* visible= */ true);
    assertCustomLinksEnabled();
    customizeShortcuts.$.hideToggle.click();
    const setSettingsCalled =
        testProxy.handler.whenCalled('setMostVisitedSettings');
    customizeShortcuts.apply();
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertTrue(customLinksEnabled);
    assertFalse(visible);
  });
});
