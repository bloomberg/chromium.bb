// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {BrowserProxy} from 'chrome://os-settings/chromeos/os_settings.js';
import {flushTasks} from 'chrome://test/test_util.js';
import {setupFakeHandler, replaceStore, replaceBody, isHiddenByDomIf} from './test_util.js';

suite('AppManagementPageTests', () => {
  let appManagementPage;
  let fakeHandler;
  let store;

  /** @return {Element} */
  function getAppList() {
    return appManagementPage.$$('app-management-main-view').$['app-list'];
  }

  /** @return {number} */
  function getAppListChildren() {
    return getAppList().childElementCount -
        1;  // Ignore the dom-repeat element.
  }

  /** @return {Element} */
  function getNoAppsFoundLabel() {
    return appManagementPage.$$('app-management-main-view')
        .$$('#no-apps-label');
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    store = replaceStore();
    appManagementPage = document.createElement('settings-app-management-page');
    assertTrue(!!appManagementPage);
    replaceBody(appManagementPage);
  });

  test('loads', async () => {
    // Check that the browser responds to the getApps() message.
    const {apps: initialApps} =
        await BrowserProxy.getInstance().handler.getApps();
  });

  test('App list renders on page change', async () => {
    await fakeHandler.addApp();
    assertEquals(1, getAppListChildren());
    await fakeHandler.addApp();
    assertEquals(2, getAppListChildren());
  });

  test('No Apps Found Label', async () => {
    assertEquals(0, getAppListChildren());
    assertFalse(isHiddenByDomIf(getNoAppsFoundLabel()));

    const app = await fakeHandler.addApp();
    assertEquals(1, getAppListChildren());
    assertTrue(isHiddenByDomIf(getNoAppsFoundLabel()));

    fakeHandler.uninstall(app.id);
    await flushTasks();
    assertEquals(0, getAppListChildren());
    assertFalse(isHiddenByDomIf(getNoAppsFoundLabel()));
  });

  test('App list filters when searching', async () => {
    await fakeHandler.addApp(null, {title: 'slides'});
    await fakeHandler.addApp(null, {title: 'calculator'});
    const sheets = await fakeHandler.addApp(null, {title: 'sheets'});
    assertEquals(3, getAppListChildren());

    appManagementPage.searchTerm = 's';
    await flushTasks();
    assertEquals(2, getAppListChildren());

    fakeHandler.uninstall(sheets.id);
    await flushTasks();
    assertEquals(1, getAppListChildren());

    appManagementPage.searchTerm = 'ss';
    await flushTasks();
    assertEquals(0, getAppListChildren());
    assertFalse(isHiddenByDomIf(getNoAppsFoundLabel()));

    appManagementPage.searchTerm = '';
    await flushTasks();
    assertEquals(2, getAppListChildren());
  });
});
