// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
testcase.deleteMenuItemNoEntrySelected = async function() {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Right click the list without selecting an entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['list.list']),
      'fakeMouseRightClick failed');

  // Wait until the context menu is shown.
  await remoteCall.waitForElement(appId, contextMenu);

  // Assert the menu delete command is disabled.
  const deleteDisabled = '[command="#delete"][disabled="disabled"]';
  await remoteCall.waitForElement(appId, contextMenu + ' ' + deleteDisabled);
};

/**
 * Tests deleting an entry using the toolbar.
 */
testcase.deleteEntryWithToolbar = async function() {
  var beforeDeletion = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]);

  var afterDeletion = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.beautiful,
  ]);

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Confirm entries in the directory before the deletion.
  await remoteCall.waitForFiles(
      appId, beforeDeletion, {ignoreLastModifiedTime: true});

  // Select My Desktop Background.png
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, ['My Desktop Background.png']));


  // Click delete button in the toolbar.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['button#delete-button']));


  // Confirm that the confirmation dialog is shown.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Press delete button.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['button.cr-dialog-ok']),
      'fakeMouseClick failed');

  // Confirm the file is removed.
  await remoteCall.waitForFiles(
      appId, afterDeletion, {ignoreLastModifiedTime: true});
};
