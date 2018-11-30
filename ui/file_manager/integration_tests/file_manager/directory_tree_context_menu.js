// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Sets up for directory tree context menu test. In addition to normal setup, we
 * add destination directory.
 */
async function setupForDirectoryTreeContextMenuTest() {
  const {appId} = await setupAndWaitUntilReady(null, RootPath.DOWNLOADS);

  // Add destination directory.
  await new addEntries(['local'], [new TestEntryInfo({
                         type: EntryType.DIRECTORY,
                         targetPath: 'destination',
                         lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                         nameText: 'destination',
                         sizeText: '--',
                         typeText: 'Folder'
                       })]);
  return appId;
}

/**
 * @const
 */
var ITEMS_IN_DEST_DIR_BEFORE_PASTE = TestEntryInfo.getExpectedRows([]);

/**
 * @const
 */
var ITEMS_IN_DEST_DIR_AFTER_PASTE =
    TestEntryInfo.getExpectedRows([new TestEntryInfo({
      type: EntryType.DIRECTORY,
      targetPath: 'photos',
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      nameText: 'photos',
      sizeText: '--',
      typeText: 'Folder'
    })]);

/**
 * Clicks context menu item of id in directory tree.
 */
async function clickDirectoryTreeContextMenuItem(appId, path, id) {
  const contextMenu = '#directory-tree-context-menu:not([hidden])';
  const pathQuery = `#directory-tree [full-path-for-testing="${path}"]`;

  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('focus', appId, [pathQuery]),
      'focus failed');

  // Right click photos directory.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, [pathQuery]),
      'fakeMouseRightClick failed');


  // Check: context menu item |id| should be shown enabled.
  await remoteCall.waitForElement(
      appId, `${contextMenu} [command="#${id}"]:not([disabled])`);

  // Click the menu item specified by |id|.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [`${contextMenu} [command="#${id}"]`]);
}

/**
 * Navigates to destination directory and test paste operation to check whether
 * the paste operation is done correctly or not. This method does NOT check
 * source entry is deleted or not for cut operation.
 */
async function navigateToDestinationDirectoryAndTestPaste(appId) {
  // Navigates to destination directory.
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/destination', 'My files/Downloads');

  // Confirm files before paste.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_BEFORE_PASTE, {ignoreLastModifiedTime: true});

  // Paste
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'v', true /* ctrl */, false, false]);

  // Confirm the photos directory is pasted correctly.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_AFTER_PASTE, {ignoreLastModifiedTime: true});
}

/**
 * Rename photos directory to specified name by using directory tree.
 */
async function renamePhotosDirectoryTo(appId, newName, useKeyboardShortcut) {
  if (useKeyboardShortcut) {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeKeyDown', appId,
        ['body', 'Enter', true /* ctrl */, false, false]));
  } else {
    await clickDirectoryTreeContextMenuItem(
        appId, RootPath.DOWNLOADS_PATH + '/photos', 'rename');
  }
  await remoteCall.waitForElement(appId, '.tree-row > input');
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['.tree-row > input', newName]);
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['.tree-row > input', 'Enter', false, false, false]);
}

/**
 * Renames directory and confirm current directory is moved to the renamed
 * directory.
 */
async function renameDirectoryFromDirectoryTreeSuccessCase(
    useKeyboardShortcut) {
  const appId = await setupForDirectoryTreeContextMenuTest();

  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  await renamePhotosDirectoryTo(appId, 'New photos', useKeyboardShortcut);

  // Confirm that current directory has moved to new folder.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads/New photos');
}

/**
 * Renames directory and confirms that an alert dialog is shown.
 */
async function renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(newName) {
  const appId = await setupForDirectoryTreeContextMenuTest();

  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  await renamePhotosDirectoryTo(appId, newName, false);

  // Confirm that a dialog is shown.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');
}

/**
 * Creates directory from directory tree.
 */
async function createDirectoryFromDirectoryTree(
    useKeyboardShortcut, changeCurrentDirectory) {
  const appId = await setupForDirectoryTreeContextMenuTest();


  if (changeCurrentDirectory) {
    await remoteCall.navigateWithDirectoryTree(
        appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  } else {
    const downloadsQuery =
        '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
    await remoteCall.expandDirectoryTreeFor(appId, downloadsQuery);
  }
  if (useKeyboardShortcut) {
    await remoteCall.callRemoteTestUtil(
        'fakeKeyDown', appId, ['body', 'e', true /* ctrl */, false, false]);
  } else {
    await clickDirectoryTreeContextMenuItem(
        appId, RootPath.DOWNLOADS_PATH + '/photos', 'new-folder');
  }
  await remoteCall.waitForElement(appId, '.tree-row > input');
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['.tree-row > input', 'test']);
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['.tree-row > input', 'Enter', false, false, false]);

  // Confirm that new directory is added to the directory tree.
  await remoteCall.waitForElement(
      appId,
      `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos/test"]`);

  // Confirm that current directory is not changed at this timing.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId,
      changeCurrentDirectory ? '/My files/Downloads/photos' :
                               '/My files/Downloads');

  // Confirm that new directory is actually created by navigating to it.
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos/test', 'My files/Downloads');
}

/**
 * Tests copying a directory from directory tree with context menu.
 */
testcase.dirCopyWithContextMenu = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  await clickDirectoryTreeContextMenuItem(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'copy');
  await navigateToDestinationDirectoryAndTestPaste(appId);
};

/**
 * Tests copying a directory from directory tree with the keyboard shortcut.
 */
testcase.dirCopyWithKeyboard = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');

  // Press Ctrl+C.
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'c', true /* ctrl */, false, false]);
  await navigateToDestinationDirectoryAndTestPaste(appId);
};

/**
 * Tests copying a directory without changing the current directory.
 */
testcase.dirCopyWithoutChangingCurrent = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const downloadsQuery =
      '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
  await remoteCall.expandTreeItemInDirectoryTree(appId, downloadsQuery);
  await clickDirectoryTreeContextMenuItem(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'copy');
  await navigateToDestinationDirectoryAndTestPaste(appId);
};

/**
 * Tests cutting a directory with the context menu.
 */
testcase.dirCutWithContextMenu = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  await clickDirectoryTreeContextMenuItem(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'cut');
  await navigateToDestinationDirectoryAndTestPaste(appId);

  // Confirm that directory tree is updated.
  await remoteCall.waitForElementLost(
      appId, `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
};

/**
 * Tests cutting a directory with the keyboard shortcut.
 */
testcase.dirCutWithKeyboard = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');

  // Press Ctrl+X.
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'x', true /* ctrl */, false, false]);
  await navigateToDestinationDirectoryAndTestPaste(appId);

  // Confirm that directory tree is updated.
  await remoteCall.waitForElementLost(
      appId, `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
};

/**
 * Tests cutting a directory without changing the current directory.
 */
testcase.dirCutWithoutChangingCurrent = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();

  const downloadsQuery =
      '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
  await remoteCall.expandTreeItemInDirectoryTree(appId, downloadsQuery);
  await clickDirectoryTreeContextMenuItem(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'cut');
  await navigateToDestinationDirectoryAndTestPaste(appId);
  await remoteCall.waitForElementLost(
      appId, `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
};

/**
 * Tests pasting into folder with the context menu.
 */
testcase.dirPasteWithContextMenu = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const destinationPath = RootPath.DOWNLOADS_PATH + '/destination';

  // Copy photos directory as a test data.
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['body', 'c', true /* ctrl */, false, false]);
  await remoteCall.navigateWithDirectoryTree(
      appId, destinationPath, 'My files/Downloads');

  // Confirm files before paste.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_BEFORE_PASTE, {ignoreLastModifiedTime: true});

  await clickDirectoryTreeContextMenuItem(
      appId, destinationPath, 'paste-into-folder');

  // Confirm the photos directory is pasted correctly.
  await remoteCall.waitForFiles(
      appId, ITEMS_IN_DEST_DIR_AFTER_PASTE, {ignoreLastModifiedTime: true});

  // Expand the directory tree.
  await remoteCall.waitForElement(
      appId, `[full-path-for-testing="${destinationPath}"] .expand-icon`);
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      [`[full-path-for-testing="${destinationPath}"] .expand-icon`]);

  // Confirm the copied directory is added to the directory tree.
  await remoteCall.waitForElement(
      appId, `[full-path-for-testing="${destinationPath}/photos"]`);
};

/**
 * Tests pasting into a folder without changing the current directory.
 */
testcase.dirPasteWithoutChangingCurrent = async function() {
  const destinationPath = RootPath.DOWNLOADS_PATH + '/destination';
  const downloadsQuery =
      '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';

  const appId = await setupForDirectoryTreeContextMenuTest();
  await remoteCall.expandDirectoryTreeFor(appId, downloadsQuery);
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [`${downloadsQuery} .expand-icon`]),
      'click on expand icon failed');
  await remoteCall.waitForElement(appId, downloadsQuery + '[expanded]');
  await remoteCall.callRemoteTestUtil('focus', appId, ['#directory-tree']);
  await clickDirectoryTreeContextMenuItem(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'copy');
  await clickDirectoryTreeContextMenuItem(
      appId, destinationPath, 'paste-into-folder');
  await remoteCall.waitForElement(
      appId, `[full-path-for-testing="${destinationPath}"][may-have-children]`);
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      [`[full-path-for-testing="${destinationPath}"] .expand-icon`]);

  // Confirm the copied directory is added to the directory tree.
  await remoteCall.waitForElement(
      appId, `[full-path-for-testing="${destinationPath}/photos"]`);
};

/**
 * Tests renaming a folder with the context menu.
 */
testcase.dirRenameWithContextMenu = function() {
  return renameDirectoryFromDirectoryTreeSuccessCase(
      false /* do not use keyboard shortcut */);
};

/**
 * Tests that a child folder breadcrumbs is updated when renaming its parent
 * folder. crbug.com/885328.
 */
testcase.dirRenameUpdateChildrenBreadcrumbs = async function() {
  const {appId} = await setupAndWaitUntilReady(null, RootPath.DOWNLOADS);

  // Add child-folder inside /photos/
  await new addEntries(['local'], [new TestEntryInfo({
                         type: EntryType.DIRECTORY,
                         targetPath: 'photos/child-folder',
                         lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                         nameText: 'child-folder',
                         sizeText: '--',
                         typeText: 'Folder'
                       })]);

  // Navigate to child folder.
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos/child-folder',
      'My files/Downloads');

  // Rename parent folder.
  await clickDirectoryTreeContextMenuItem(
      appId, RootPath.DOWNLOADS_PATH + '/photos', 'rename');
  await remoteCall.waitForElement(appId, '.tree-row > input');
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['.tree-row > input', 'photos-new']);
  const enterKey = ['.tree-row > input', 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, enterKey),
      'Enter key failed');


  // Confirm that current directory is now My files or /Downloads, because it
  // can't find the previously selected folder /Downloads/photos/child-folder,
  // since its path/parent has been renamed.
  // TODO(lucmult): Remove this conditional once MyFilesVolume is rolled out.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId,
      RootPath.DOWNLOADS_PATH === '/Downloads' ? '/My files' :
                                                 '/My files/Downloads');

  // Navigate to child-folder using the new path.
  // |navigateWithDirectoryTree| already checks for breadcrumbs to
  // match the path.
  await remoteCall.navigateWithDirectoryTree(
      appId, RootPath.DOWNLOADS_PATH + '/photos-new/child-folder',
      'My files/Downloads');
};

/**
 * Tests renaming folder with the keyboard shortcut.
 */
testcase.dirRenameWithKeyboard = function() {
  return renameDirectoryFromDirectoryTreeSuccessCase(
      true /* use keyboard shortcut */);
};

/**
 * Tests renaming folder without changing the current directory.
 */
testcase.dirRenameWithoutChangingCurrent = async function() {
  const appId = await setupForDirectoryTreeContextMenuTest();
  const downloadsQuery =
      '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
  await remoteCall.expandTreeItemInDirectoryTree(appId, downloadsQuery);
  await remoteCall.waitForElement(
      appId, `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
  await renamePhotosDirectoryTo(
      appId, 'New photos', false /* Do not use keyboard shortcut. */);
  await remoteCall.waitForElementLost(
      appId, `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
  await remoteCall.waitForElement(
      appId, `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/New photos"]`);
};

/**
 * Tests renaming a folder to an empty string.
 */
testcase.dirRenameToEmptyString = function() {
  return renameDirectoryFromDirectoryTreeAndConfirmAlertDialog('');
};

/**
 * Tests renaming folder an existing name.
 */
testcase.dirRenameToExisting = function() {
  return renameDirectoryFromDirectoryTreeAndConfirmAlertDialog('destination');
};

/**
 * Tests creating a folder with the context menu.
 */
testcase.dirCreateWithContextMenu = function() {
  return createDirectoryFromDirectoryTree(
      false /* do not use keyboard shortcut */,
      true /* change current directory */);
};

/**
 * Tests creating a folder with the keyboard shortcut.
 */
testcase.dirCreateWithKeyboard = function() {
  return createDirectoryFromDirectoryTree(
      true /* use keyboard shortcut */, true /* change current directory */);
};

/**
 * Tests creating folder without changing the current directory.
 */
testcase.dirCreateWithoutChangingCurrent = function() {
  return createDirectoryFromDirectoryTree(
      false /* Do not use keyboard shortcut */,
      false /* Do not change current directory */);
};
