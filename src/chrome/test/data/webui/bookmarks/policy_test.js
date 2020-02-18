// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, Command, CommandManager, IncognitoAvailability} from 'chrome://bookmarks/bookmarks.js';
import {TestBookmarksBrowserProxy} from 'chrome://test/bookmarks/test_browser_proxy.js';
import {TestStore} from 'chrome://test/bookmarks/test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from 'chrome://test/bookmarks/test_util.js';

suite('Bookmarks policies', function() {
  let store;
  let app;
  /** @type {?bookmarks.BrowserProxy} */
  let testBrowserProxy;

  setup(function() {
    const nodes = testTree(createFolder('1', [
      createItem('11'),
    ]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.expectAction('set-incognito-availability');
    store.expectAction('set-can-edit');
    store.replaceSingleton();

    testBrowserProxy = new TestBookmarksBrowserProxy();
    BrowserProxy.instance_ = testBrowserProxy;
    app = document.createElement('bookmarks-app');
    replaceBody(app);
  });

  test('incognito availability updates when changed', async function() {
    const commandManager = CommandManager.getInstance();
    // Incognito is disabled during testGenPreamble(). Wait for the front-end to
    // load the config.
    const whenIncognitoSet = await Promise.all([
      testBrowserProxy.whenCalled('getIncognitoAvailability'),
      store.waitForAction('set-incognito-availability')
    ]);

    assertEquals(
        IncognitoAvailability.DISABLED, store.data.prefs.incognitoAvailability);
    assertFalse(
        commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));

    cr.webUIListenerCallback(
        'incognito-availability-changed', IncognitoAvailability.ENABLED);
    assertEquals(
        IncognitoAvailability.ENABLED, store.data.prefs.incognitoAvailability);
    assertTrue(
        commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));
  });

  test('canEdit updates when changed', async function() {
    const commandManager = CommandManager.getInstance();
    const whenCanEditSet = await Promise.all([
      testBrowserProxy.whenCalled('getCanEditBookmarks'),
      store.waitForAction('set-can-edit')
    ]);
    assertFalse(store.data.prefs.canEdit);
    assertFalse(commandManager.canExecute(Command.DELETE, new Set(['11'])));

    cr.webUIListenerCallback('can-edit-bookmarks-changed', true);
    assertTrue(store.data.prefs.canEdit);
    assertTrue(commandManager.canExecute(Command.DELETE, new Set(['11'])));
  });
});
