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
 * TODO(sashab): Once Shared drives is enabled, add tests for team drive roots
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
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPLEX_DRIVE_ENTRY_SET);

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
testcase.checkDeleteEnabledForReadWriteFile = () => {
  return checkContextMenu('delete', 'hello.txt', true);
};

/**
 * Tests that the Delete menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyDocument = () => {
  return checkContextMenu('delete', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only file is selected.
 */
testcase.checkDeleteDisabledForReadOnlyFile = () => {
  return checkContextMenu('delete', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyFolder = () => {
  return checkContextMenu('delete', 'Read-Only Folder', false);
};

/**
 * Tests that the Rename menu item is enabled if a read-write entry is selected.
 */
testcase.checkRenameEnabledForReadWriteFile = () => {
  return checkContextMenu('rename', 'hello.txt', true);
};

/**
 * Tests that the Rename menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyDocument = () => {
  return checkContextMenu('rename', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only file is selected.
 */
testcase.checkRenameDisabledForReadOnlyFile = () => {
  return checkContextMenu('rename', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyFolder = () => {
  return checkContextMenu('rename', 'Read-Only Folder', false);
};

/**
 * Tests that the Share menu item is enabled if a read-write entry is selected.
 */
testcase.checkShareEnabledForReadWriteFile = () => {
  return checkContextMenu('share', 'hello.txt', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyDocument = () => {
  return checkContextMenu('share', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Share menu item is disabled if a strict read-only document is
 * selected.
 */
testcase.checkShareDisabledForStrictReadOnlyDocument = () => {
  return checkContextMenu('share', 'Read-Only (Strict) Doc.gdoc', false);
};

/**
 * Tests that the Share menu item is enabled if a read-only file is selected.
 */
testcase.checkShareEnabledForReadOnlyFile = () => {
  return checkContextMenu('share', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyFolder = () => {
  return checkContextMenu('share', 'Read-Only Folder', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-write entry is selected.
 */
testcase.checkCopyEnabledForReadWriteFile = () => {
  return checkContextMenu('copy', 'hello.txt', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyDocument = () => {
  return checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is disabled if a strict (no-copy) read-only
 * document is selected.
 */
testcase.checkCopyDisabledForStrictReadOnlyDocument = () => {
  return checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only file is selected.
 */
testcase.checkCopyEnabledForReadOnlyFile = () => {
  return checkContextMenu('copy', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyFolder = () => {
  return checkContextMenu('copy', 'Read-Only Folder', true);
};

/**
 * Tests that the Cut menu item is enabled if a read-write entry is selected.
 */
testcase.checkCutEnabledForReadWriteFile = () => {
  return checkContextMenu('cut', 'hello.txt', true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyDocument = () => {
  return checkContextMenu('cut', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only file is selected.
 */
testcase.checkCutDisabledForReadOnlyFile = () => {
  return checkContextMenu('cut', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyFolder = () => {
  return checkContextMenu('cut', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste into Folder menu item is enabled if a read-write folder
 * is selected.
 */
testcase.checkPasteIntoFolderEnabledForReadWriteFolder = () => {
  return checkContextMenu('paste-into-folder', 'photos', true);
};

/**
 * Tests that the Paste into Folder menu item is disabled if a read-only folder
 * is selected.
 */
testcase.checkPasteIntoFolderDisabledForReadOnlyFolder = () => {
  return checkContextMenu('paste-into-folder', 'Read-Only Folder', false);
};

/**
 * Tests that text selection context menus are disabled in tablet mode.
 */
testcase.checkContextMenusForInputElements = async () => {
  // Open FilesApp on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPLEX_DRIVE_ENTRY_SET);

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
testcase.checkNewFolderEnabledInsideReadWriteFolder = () => {
  return checkContextMenuInDriveFolder('new-folder', 'photos', true);
};

/**
 * Tests that the New Folder menu item is enabled inside a folder that has
 * read-write permissions.
 */
testcase.checkNewFolderDisabledInsideReadOnlyFolder = () => {
  return checkContextMenuInDriveFolder('new-folder', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste menu item is enabled inside a folder that has read-write
 * permissions.
 */
testcase.checkPasteEnabledInsideReadWriteFolder = () => {
  return checkContextMenuInDriveFolder('paste', 'photos', true);
};

/**
 * Tests that the Paste menu item is disabled inside a folder that has read-only
 * permissions.
 */
testcase.checkPasteDisabledInsideReadOnlyFolder = () => {
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
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPLEX_DRIVE_ENTRY_SET);

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
testcase.checkCopyEnabledForReadWriteFolderInTree = () => {
  return checkContextMenuForDriveFolderInTree(
      'copy',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected in the directory tree.
 */
testcase.checkCopyEnabledForReadOnlyFolderInTree = () => {
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
testcase.checkCutEnabledForReadWriteFolderInTree = () => {
  return checkContextMenuForDriveFolderInTree(
      'cut',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected in the directory tree.
 */
testcase.checkCutDisabledForReadOnlyFolderInTree = () => {
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
testcase.checkPasteEnabledForReadWriteFolderInTree = () => {
  return checkContextMenuForDriveFolderInTree(
      'paste-into-folder',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Paste menu item is disabled in the directory tree for a folder
 * that has read-only permissions.
 */
testcase.checkPasteDisabledForReadOnlyFolderInTree = () => {
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
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], TEAM_DRIVE_ENTRY_SET);

  // Focus the file list.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#file-list:not([hidden])']));

  // Select 'Shared drives'.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'selectFolderInTree', appId, ['Shared drives']));

  // Expand 'Shared drives'.
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
testcase.checkContextMenuForTeamDriveRoot = () => {
  return checkTeamDriveContextMenuInTree('Team Drive A', {
    'cut': false,
    'copy': false,
    'rename': false,
    'delete': false,
    'new-folder': true
  });
};

/**
 * Checks that mutating context menu items are not present for a root within
 * My files.
 * @param {string} itemName Name of item inside MyFiles that should be checked.
 * @param {!Object<string, boolean>} commandStates Commands that should be
 *     enabled for the checked item.
 */
async function checkMyFilesRootItemContextMenu(itemName, commandStates) {
  const validCmds = {
    'copy': true,
    'cut': true,
    'delete': true,
    'rename': true,
    'zip-selection': true,
  };

  const enabledCmds = [];
  const disabledCmds = [];
  for (let [cmd, enabled] of Object.entries(commandStates)) {
    chrome.test.assertTrue(cmd in validCmds, cmd + ' is not a valid command.');
    if (enabled) {
      enabledCmds.push(cmd);
    } else {
      disabledCmds.push(cmd);
    }
  }

  // Open FilesApp on Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Navigate to My files.
  await remoteCall.waitAndClickElement(appId, '#breadcrumb-path-0');

  // Wait for the navigation to complete.
  const expectedRows = [
    ['Downloads', '--', 'Folder'],
    ['Play files', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows,
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Select the item.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [itemName]));

  // Wait for the file to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Right-click the selected file.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Check the enabled commands.
  for (const commandId of enabledCmds) {
    let query = `#file-context-menu:not([hidden]) [command="#${
        commandId}"]:not([disabled])`;
    await remoteCall.waitForElement(appId, query);
  }

  // Check the disabled commands.
  for (const commandId of disabledCmds) {
    let query =
        `#file-context-menu:not([hidden]) [command="#${commandId}"][disabled]`;
    await remoteCall.waitForElement(appId, query);
  }

  // Check that the delete button isn't visible.
  const deleteButton = await remoteCall.waitForElement(appId, '#delete-button');
  chrome.test.assertTrue(deleteButton.hidden, 'delete button should be hidden');
}

/**
 * Check that mutating context menu items are not shown for Downloads within My
 * files.
 */
testcase.checkDownloadsContextMenu = () => {
  const commands = {
    copy: true,
    cut: false,
    delete: false,
    rename: false,
    'zip-selection': true,
  };
  return checkMyFilesRootItemContextMenu('Downloads', commands);
};

/**
 * Check that mutating context menu items are not shown for Play files within My
 * files.
 */
testcase.checkPlayFilesContextMenu = () => {
  const commands = {
    copy: false,
    cut: false,
    delete: false,
    rename: false,
    'zip-selection': false,
  };
  return checkMyFilesRootItemContextMenu('Play files', commands);
};

/**
 * Check that mutating context menu items are not shown for Linux files within
 * My files.
 */
testcase.checkLinuxFilesContextMenu = () => {
  const commands = {
    copy: false,
    cut: false,
    delete: false,
    rename: false,
    'zip-selection': false,
  };
  return checkMyFilesRootItemContextMenu('Linux files', commands);
};

/**
 * Checks the unmount command is visible on the roots context menu for a
 * specified removable directory entry.
 */
async function checkUnmountRootsContextMenu(entryLabel) {
  // Query the element by label, and wait for the contextmenu attribute which
  // shows the menu has been set up.
  const query = `#directory-tree [entry-label="${entryLabel}"][contextmenu]`;

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount removable volumes.
  await sendTestMessage({name: 'mountUsbWithPartitions'});
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for removable volume to appear in the directory tree.
  const removable = await remoteCall.waitForElement(appId, query);

  // Right-click on the removable volume.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, [query]));

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#roots-context-menu:not([hidden])');

  // Check the unmount command is visible in the context menu.
  const commandQuery =
      '#roots-context-menu:not([hidden]) [command="#unmount"]:not([hidden])';
  await remoteCall.waitForElement(appId, commandQuery);
}

/**
 * Checks that the unmount command is shown in the context menu for a removable
 * root with child partitions.
 */
testcase.checkRemovableRootContextMenu = async () => {
  return checkUnmountRootsContextMenu('Drive Label');
};

/**
 * Checks that the unmount command is shown in the context menu for a USB.
 */
testcase.checkUsbContextMenu = async () => {
  return checkUnmountRootsContextMenu('fake-usb');
};

/**
 * Checks the roots context menu does not appear for a removable partition,
 * The directory tree context menu should be visible and display the new-folder
 * command.
 */
testcase.checkPartitionContextMenu = async () => {
  // Query the element by label, and wait for the contextmenu attribute which
  // shows the menu has been set up.
  const partitionQuery = '#directory-tree .tree-children ' +
      '[entry-label="partition-1"][contextmenu] ' +
      '[volume-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount removable volumes.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for partition-1 to appear in the directory tree.
  const removable = await remoteCall.waitForElement(appId, partitionQuery);

  // Right-click on the partition.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, [partitionQuery]));

  // Check the root context menu is hidden so there is no option to eject.
  await remoteCall.waitForElement(appId, '#roots-context-menu[hidden]');

  // Check the command to create a new-folder is visible from the directory
  // tree context menu.
  await remoteCall.waitForElement(
      appId,
      '#directory-tree-context-menu:not([hidden]) ' +
          '[command="#new-folder"]:not([hidden])');
};
