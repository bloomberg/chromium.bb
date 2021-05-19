// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
testcase.toolbarDeleteWithMenuItemNoEntrySelected = async () => {
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
 * Tests that the toolbar Delete button opens the delete confirm dialog and
 * that the dialog cancel button has the focus by default.
 */
testcase.toolbarDeleteButtonOpensDeleteConfirmDialog = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], [ENTRIES.desktop]);

  // Select My Desktop Background.png
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [ENTRIES.desktop.nameText]));

  // Click the toolbar Delete button.
  await remoteCall.simulateUiClick(appId, '#delete-button');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const defaultDialogButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', defaultDialogButton.text);
};

/**
 * Tests that the toolbar Delete button keeps focus after the delete confirm
 * dialog is closed.
 */
testcase.toolbarDeleteButtonKeepFocus = async () => {
  // Open Files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // USB delete never uses trash and always shows the delete dialog.
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY]),
      'fakeMouseClick failed');

  // Check: the USB files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Select hello.txt
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [ENTRIES.hello.nameText]));

  // Click the toolbar Delete button.
  await remoteCall.simulateUiClick(appId, '#delete-button');

  // Check: the Delete button should lose focus.
  await remoteCall.waitForElementLost(appId, '#delete-button:focus');

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Check: the dialog 'Cancel' button should be focused by default.
  const defaultDialogButton =
      await remoteCall.waitForElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', defaultDialogButton.text);

  // Click the dialog 'Cancel' button.
  await remoteCall.waitAndClickElement(appId, '.cr-dialog-cancel');

  // Check: the toolbar Delete button should be focused.
  await remoteCall.waitForElement(appId, '#delete-button:focus');
};

/**
 * Tests deleting an entry using the toolbar.
 */
testcase.toolbarDeleteEntry = async () => {
  const beforeDeletion = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]);

  const afterDeletion = TestEntryInfo.getExpectedRows([
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
  if (await sendTestMessage({name: 'isTrashEnabled'}) === 'true') {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#move-to-trash-button']));
  } else {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#delete-button']));

    // Confirm that the confirmation dialog is shown.
    await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

    // Press delete button.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId, ['button.cr-dialog-ok']),
        'fakeMouseClick failed');
  }

  // Confirm the file is removed.
  await remoteCall.waitForFiles(
      appId, afterDeletion, {ignoreLastModifiedTime: true});
};

/**
 * Tests that refresh button hides in selection mode.
 * Non-watchable volumes display refresh button so users can refresh the file
 * list content. However this button should be hidden when entering the
 * selection mode. crbug.com/978383
 *
 */
testcase.toolbarRefreshButtonWithSelection = async () => {
  // Enable media views which are non-watchable.
  await sendTestMessage({name: 'mountMediaView'});

  // Add some content to media view "Images".
  await addEntries(['media_view_images'], [ENTRIES.desktop]);

  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Navigate to Images media view.
  await remoteCall.waitAndClickElement(
      appId, '#directory-tree [entry-label="Images"]');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Images');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');

  // Ctrl+A to enter selection mode.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that refresh button is not shown when Recent is selected.
 */
testcase.toolbarRefreshButtonHiddenInRecents = async () => {
  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Navigate to Recent.
  await remoteCall.waitAndClickElement(
      appId, '#directory-tree [entry-label="Recent"]');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Recent');

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that command Alt+A focus the toolbar.
 */
testcase.toolbarAltACommand = async () => {
  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Press Alt+A in the File List.
  const altA = ['#file-list', 'a', false, false, true];
  await remoteCall.fakeKeyDown(appId, ...altA);

  // Check that a menu-button should be focused.
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  const cssClasses = focusedElement.attributes['class'] || '';
  chrome.test.assertTrue(cssClasses.includes('menu-button'));
};

/**
 * Tests that the menu drop down follows the button if the button moves. This
 * happens when the search box is expanded and then collapsed.
 */
testcase.toolbarMultiMenuFollowsButton = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Override the tasks so the "Open" button becomes a dropdown button.
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Select an entry in the file list.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Click the toolbar "Open" dropdown button.
  await remoteCall.simulateUiClick(appId, '#tasks');

  // Wait for the search box to collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Check that the dropdown menu and "Open" button are aligned.
  const caller = getCaller();
  await repeatUntil(async () => {
    const openButton =
        await remoteCall.waitForElementStyles(appId, '#tasks', ['width']);
    const menu =
        await remoteCall.waitForElementStyles(appId, '#tasks-menu', ['width']);

    if (openButton.renderedLeft !== menu.renderedLeft) {
      return pending(
          caller,
          `Waiting for the menu and button to be aligned: ` +
              `${openButton.renderedLeft} != ${menu.renderedLeft}`);
    }
  });
};

/**
 * Tests that the sharesheet button is enabled and executable.
 */
testcase.toolbarSharesheetButtonWithSelection = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  let fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Fake chrome.fileManagerPrivate.invokeSharesheet.
  fakeData = {
    'chrome.fileManagerPrivate.invokeSharesheet': ['static_fake', []],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  const entry = ENTRIES.hello;

  // Select an entry in the file list.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  await remoteCall.waitAndClickElement(
      appId, '#sharesheet-button:not([hidden])');

  // Check invoke sharesheet is called.
  chrome.test.assertEq(
      1, await remoteCall.callRemoteTestUtil('staticFakeCounter', appId, [
        'chrome.fileManagerPrivate.invokeSharesheet'
      ]));

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(2, removedCount);
};

/**
 * Tests that the sharesheet command in context menu is enabled and executable.
 */
testcase.toolbarSharesheetContextMenuWithSelection = async () => {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  let fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Fake chrome.fileManagerPrivate.invokeSharesheet.
  fakeData = {
    'chrome.fileManagerPrivate.invokeSharesheet': ['static_fake', []],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  const entry = ENTRIES.hello;

  // Select an entry in the file list.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  chrome.test.assertTrue(!!await remoteCall.waitAndRightClick(
      appId, '#file-list .table-row[selected]'));

  // Wait until the context menu is shown.
  await remoteCall.waitForElement(appId, contextMenu);

  // Assert the menu sharesheet command is not hidden.
  const sharesheetEnabled =
      '[command="#invoke-sharesheet"]:not([hidden]):not([disabled])';

  await remoteCall.waitAndClickElement(
      appId, contextMenu + ' ' + sharesheetEnabled);

  // Check invoke sharesheet is called.
  chrome.test.assertEq(
      1, await remoteCall.callRemoteTestUtil('staticFakeCounter', appId, [
        'chrome.fileManagerPrivate.invokeSharesheet'
      ]));

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(2, removedCount);
};

/**
 * Tests that the sharesheet item is hidden if no entry is selected.
 */
testcase.toolbarSharesheetNoEntrySelected = async () => {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Fake chrome.fileManagerPrivate.sharesheetHasTargets to return true.
  const fakeData = {
    'chrome.fileManagerPrivate.sharesheetHasTargets': ['static_fake', [true]],
  };
  await remoteCall.callRemoteTestUtil('foregroundFake', appId, [fakeData]);

  // Right click the list without selecting an entry.
  chrome.test.assertTrue(
      !!await remoteCall.waitAndRightClick(appId, 'list.list'));

  // Wait until the context menu is shown.
  await remoteCall.waitForElement(appId, contextMenu);

  // Assert the menu sharesheet command is disabled.
  const sharesheetDisabled =
      '[command="#invoke-sharesheet"][hidden][disabled="disabled"]';
  await remoteCall.waitForElement(
      appId, contextMenu + ' ' + sharesheetDisabled);

  await remoteCall.waitForElement(appId, '#sharesheet-button[hidden]');

  // Remove fakes.
  const removedCount = await remoteCall.callRemoteTestUtil(
      'removeAllForegroundFakes', appId, []);
  chrome.test.assertEq(1, removedCount);
};
