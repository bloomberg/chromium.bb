// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests that check the context menu displays the right options (enabled and
 * disabled) for different types of files.
 *
 * The names passed to the tests are file names to select. They are generated
 * from COMPLEX_DRIVE_ENTRY_SET (see setupAndWaitUntilReady).
 *
 * TODO(sashab): Generate the entries used in these tests at runtime, by
 * creating entries with pre-set combinations of permissions and ensuring the
 * outcome is always as expected.
 *
 * TODO(sashab): Once Team Drives is enabled, add tests for team drive roots
 * and entries as well.
 */

/**
 * Copy a text file to clipboard if the test requires it.
 *
 * @param {string} appId ID of the app window.
 * @param {string} commandId ID of the command in the context menu to check.
 */
async function maybeCopyToClipboard(appId, commandId, file = 'hello.txt') {
  if (!/^paste/.test(commandId)) {
    return;
  }
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [file]),
      'selectFile failed');
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']),
      'execCommand failed');
}

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * entry at |path| is selected.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} path Path to the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 */
async function checkContextMenu(commandId, path, expectedEnabledState) {
  // Open Files App on Drive.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], COMPLEX_DRIVE_ENTRY_SET);

  // Optionally copy hello.txt into the clipboard if needed.
  await maybeCopyToClipboard(appId, commandId);

  // Select the file |path|.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [path]));

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the command option to appear.
  let query = '#file-context-menu:not([hidden])';
  if (expectedEnabledState) {
    query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
  } else {
    query += ` [command="#${commandId}"][disabled]:not([hidden])`;
  }
  await remoteCall.waitForElement(appId, query);
}

/**
 * Tests that the Delete menu item is enabled if a read-write entry is selected.
 */
testcase.checkDeleteEnabledForReadWriteFile = function() {
  return checkContextMenu('delete', 'hello.txt', true);
};

/**
 * Tests that the Delete menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyDocument = function() {
  return checkContextMenu('delete', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only file is selected.
 */
testcase.checkDeleteDisabledForReadOnlyFile = function() {
  return checkContextMenu('delete', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyFolder = function() {
  return checkContextMenu('delete', 'Read-Only Folder', false);
};

/**
 * Tests that the Rename menu item is enabled if a read-write entry is selected.
 */
testcase.checkRenameEnabledForReadWriteFile = function() {
  return checkContextMenu('rename', 'hello.txt', true);
};

/**
 * Tests that the Rename menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyDocument = function() {
  return checkContextMenu('rename', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only file is selected.
 */
testcase.checkRenameDisabledForReadOnlyFile = function() {
  return checkContextMenu('rename', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyFolder = function() {
  return checkContextMenu('rename', 'Read-Only Folder', false);
};

/**
 * Tests that the Share menu item is enabled if a read-write entry is selected.
 */
testcase.checkShareEnabledForReadWriteFile = function() {
  return checkContextMenu('share', 'hello.txt', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyDocument = function() {
  return checkContextMenu('share', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Share menu item is disabled if a strict read-only document is
 * selected.
 */
testcase.checkShareDisabledForStrictReadOnlyDocument = function() {
  return checkContextMenu('share', 'Read-Only (Strict) Doc.gdoc', false);
};

/**
 * Tests that the Share menu item is enabled if a read-only file is selected.
 */
testcase.checkShareEnabledForReadOnlyFile = function() {
  return checkContextMenu('share', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyFolder = function() {
  return checkContextMenu('share', 'Read-Only Folder', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-write entry is selected.
 */
testcase.checkCopyEnabledForReadWriteFile = function() {
  return checkContextMenu('copy', 'hello.txt', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyDocument = function() {
  return checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is disabled if a strict (no-copy) read-only
 * document is selected.
 */
testcase.checkCopyDisabledForStrictReadOnlyDocument = function() {
  return checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only file is selected.
 */
testcase.checkCopyEnabledForReadOnlyFile = function() {
  return checkContextMenu('copy', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyFolder = function() {
  return checkContextMenu('copy', 'Read-Only Folder', true);
};

/**
 * Tests that the Cut menu item is enabled if a read-write entry is selected.
 */
testcase.checkCutEnabledForReadWriteFile = function() {
  return checkContextMenu('cut', 'hello.txt', true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyDocument = function() {
  return checkContextMenu('cut', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only file is selected.
 */
testcase.checkCutDisabledForReadOnlyFile = function() {
  return checkContextMenu('cut', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyFolder = function() {
  return checkContextMenu('cut', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste into Folder menu item is enabled if a read-write folder
 * is selected.
 */
testcase.checkPasteIntoFolderEnabledForReadWriteFolder = function() {
  return checkContextMenu('paste-into-folder', 'photos', true);
};

/**
 * Tests that the Paste into Folder menu item is disabled if a read-only folder
 * is selected.
 */
testcase.checkPasteIntoFolderDisabledForReadOnlyFolder = function() {
  return checkContextMenu('paste-into-folder', 'Read-Only Folder', false);
};

/**
 * Tests that text selection context menus are disabled in tablet mode.
 */
testcase.checkContextMenusForInputElements = async function() {
  // Open FilesApp on Downloads.
  const {appId} = await setupAndWaitUntilReady(null, RootPath.DOWNLOADS);

  // Query all input elements.
  const elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId,
      ['input[type=text], input[type=search], textarea, cr-input']);

  // Focus the search box.
  chrome.test.assertEq(2, elements.length);
  for (let element of elements) {
    chrome.test.assertEq('#text-context-menu', element.attributes.contextmenu);
  }

  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['#search-box cr-input', 'test.pdf']);

  // Notify the element of the input.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']));

  // Do the touch.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeTouchClick', appId, ['#search-box cr-input']));

  // Context menu must be hidden if touch induced.
  await remoteCall.waitForElement(appId, '#text-context-menu[hidden]');

  // Do the right click.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#search-box cr-input']));

  // Context menu must be visible if mouse induced.
  await remoteCall.waitForElement(appId, '#text-context-menu:not([hidden])');
};

/**
 * TODO(sashab): Add tests for copying to/from the directory tree on the LHS.
 */

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * context menu is opened from the file list inside the folder called
 * |folderName|. The folder is opened and the white area inside the folder is
 * selected. |folderName| must be inside the Google Drive root.
 * TODO(sashab): Allow specifying a generic path to any folder in the tree.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} folderName Path to the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 */
async function checkContextMenuInDriveFolder(
    commandId, folderName, expectedEnabledState) {
  // Open Files App on Drive.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], COMPLEX_DRIVE_ENTRY_SET);

  // Optionally copy hello.txt into the clipboard if needed.
  await maybeCopyToClipboard(appId, commandId);

  // Focus the file list.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#file-list:not([hidden])']));

  // Select 'My Drive'.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'selectFolderInTree', appId, ['My Drive']));

  // Expand 'My Drive'.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'expandSelectedFolderInTree', appId, []));

  // Select the folder.
  await remoteCall.callRemoteTestUtil(
      'selectFolderInTree', appId, [folderName]);

  // Right-click inside the file list.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#file-list']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the command option to appear.
  let query = '#file-context-menu:not([hidden])';
  if (expectedEnabledState) {
    query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
  } else {
    query += ` [command="#${commandId}"][disabled]:not([hidden])`;
  }
  await remoteCall.waitForElement(appId, query);
}

/**
 * Tests that the New Folder menu item is enabled inside a folder that has
 * read-write permissions.
 */
testcase.checkNewFolderEnabledInsideReadWriteFolder = function() {
  return checkContextMenuInDriveFolder('new-folder', 'photos', true);
};

/**
 * Tests that the New Folder menu item is enabled inside a folder that has
 * read-write permissions.
 */
testcase.checkNewFolderDisabledInsideReadOnlyFolder = function() {
  return checkContextMenuInDriveFolder('new-folder', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste menu item is enabled inside a folder that has read-write
 * permissions.
 */
testcase.checkPasteEnabledInsideReadWriteFolder = function() {
  return checkContextMenuInDriveFolder('paste', 'photos', true);
};

/**
 * Tests that the Paste menu item is disabled inside a folder that has read-only
 * permissions.
 */
testcase.checkPasteDisabledInsideReadOnlyFolder = function() {
  return checkContextMenuInDriveFolder('paste', 'Read-Only Folder', false);
};

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * context menu is opened from the directory tree. The tree item must be
 * visible.
 * TODO(sashab): Allow specifying a generic path to any folder in the tree.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} folderSelector CSS selector to the folder node in the tree.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 */
async function checkContextMenuForDriveFolderInTree(
    commandId, folderSelector, expectedEnabledState) {
  // Open Files App on Drive.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], COMPLEX_DRIVE_ENTRY_SET);

  // Optionally copy hello.txt into the clipboard if needed.
  await maybeCopyToClipboard(appId, commandId);

  // Focus the file list.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#file-list:not([hidden])']));

  // Select 'My Drive'.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'selectFolderInTree', appId, ['My Drive']));

  // Expand 'My Drive'.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'expandSelectedFolderInTree', appId, []));

  // Wait for the folder to be visible.
  await remoteCall.waitForElement(appId, `${folderSelector}:not([hidden])`);

  // Focus the selected folder.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#directory-tree']));

  // Right-click the selected folder.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId,
      [`${folderSelector}:not([hidden]) .label`]));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu:not([hidden])');

  // Wait for the command option to appear.
  let query = '#directory-tree-context-menu:not([hidden])';
  if (expectedEnabledState) {
    query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
  } else {
    query += ` [command="#${commandId}"][disabled]:not([hidden])`;
  }
  await remoteCall.waitForElement(appId, query);
}

/**
 * Tests that the Copy menu item is enabled if a read-write folder is selected
 * in the directory tree.
 */
testcase.checkCopyEnabledForReadWriteFolderInTree = function() {
  return checkContextMenuForDriveFolderInTree(
      'copy',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected in the directory tree.
 */
testcase.checkCopyEnabledForReadOnlyFolderInTree = function() {
  return checkContextMenuForDriveFolderInTree(
      'copy',
      '#directory-tree [full-path-for-testing="/root/Read-Only Folder"]' +
          ':not([hidden])',
      true);
};

/**
 * Tests that the Cut menu item is enabled if a read-write folder is
 * selected in the directory tree.
 */
testcase.checkCutEnabledForReadWriteFolderInTree = function() {
  return checkContextMenuForDriveFolderInTree(
      'cut',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected in the directory tree.
 */
testcase.checkCutDisabledForReadOnlyFolderInTree = function() {
  return checkContextMenuForDriveFolderInTree(
      'cut',
      '#directory-tree [full-path-for-testing="/root/Read-Only Folder"]' +
          ':not([hidden])',
      false);
};

/**
 * Tests that the Paste menu item is enabled in the directory
 * tree for a folder that has read-write permissions.
 */
testcase.checkPasteEnabledForReadWriteFolderInTree = function() {
  return checkContextMenuForDriveFolderInTree(
      'paste-into-folder',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Paste menu item is disabled in the directory tree for a folder
 * that has read-only permissions.
 */
testcase.checkPasteDisabledForReadOnlyFolderInTree = function() {
  return checkContextMenuForDriveFolderInTree(
      'paste-into-folder',
      '#directory-tree [full-path-for-testing="/root/Read-Only Folder"]' +
          ':not([hidden])',
      false);
};

/**
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * context menu for the Team Drive root with name |teamDriveName| is opened in
 * the directory tree.
 *
 * TODO(sashab): Make this take a map of {commandId: expectedEnabledState}, and
 * flatten all tests into 1.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} teamDriveName Team drive name to open the context menu for.
 * @param {Object} expectedContextMenuState Map of context-menu options to True
 *     if the command should be enabled in the context menu, false if not.
 */
async function checkTeamDriveContextMenuInTree(
    teamDriveName, expectedContextMenuState) {
  let navItemSelector = `#directory-tree ` +
      `.tree-item[full-path-for-testing="/team_drives/${teamDriveName}"]`;

  // Open Files App on Drive.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], TEAM_DRIVE_ENTRY_SET);

  // Focus the file list.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#file-list:not([hidden])']));

  // Select 'Team Drives'.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'selectFolderInTree', appId, ['Team Drives']));

  // Expand 'Team Drives'.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'expandSelectedFolderInTree', appId, []));

  // Wait for the team drive to be visible.
  await remoteCall.waitForElement(appId, `${navItemSelector}:not([hidden])`);

  // Focus the selected team drive.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('focus', appId, ['#directory-tree']));

  // Right-click the selected team drive.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId,
      [`${navItemSelector}:not([hidden]) .label`]));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(
      appId, '#directory-tree-context-menu:not([hidden])');

  // Wait for the command options to appear.
  let promises = [];
  for (let commandId in expectedContextMenuState) {
    let query = '#directory-tree-context-menu:not([hidden])';
    if (expectedContextMenuState[commandId] == true) {
      query += ` [command="#${commandId}"]:not([hidden]):not([disabled])`;
    } else {
      query += ` [command="#${commandId}"][disabled]:not([hidden])`;
    }
    promises.push(remoteCall.waitForElement(appId, query));
  }

  await Promise.all(promises);
}

/**
 * Tests that the context menu contains the correct items with the correct
 * enabled/disabled state if a Team Drive Root is selected.
 */
testcase.checkContextMenuForTeamDriveRoot = function() {
  return checkTeamDriveContextMenuInTree('Team Drive A', {
    'cut': false,
    'copy': false,
    'rename': false,
    'delete': false,
    'new-folder': true
  });
};
