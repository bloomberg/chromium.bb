// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
 * Tests that the specified menu item is in |expectedEnabledState| when the
 * entry at |path| is selected.
 *
 * @param {string} commandId ID of the command in the context menu to check.
 * @param {string} path Path to the file to open the context menu for.
 * @param {boolean} expectedEnabledState True if the command should be enabled
 *     in the context menu, false if not.
 */
function checkContextMenu(commandId, path, expectedEnabledState) {
  let appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          COMPLEX_DRIVE_ENTRY_SET);
    },
    // Select the file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, [path], this.next);
    },
    // Wait for the file to be selected.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElement(appId, '.table-row[selected]').then(this.next);
    },
    // Right-click on the file.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the command option to appear.
    function() {
      let query;
      if (expectedEnabledState) {
        query = `[command="#${commandId}"]:not([hidden]):not([disabled])`;
      } else {
        query = `[command="#${commandId}"][disabled]:not([hidden])`;
      }
      remoteCall.waitForElement(appId, query).then(this.next);
    },
    // Check for Javascript errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Right-clicks on the specified item and selects "Copy".
 *
 * @param {string} path Path to the file or folder to copy.
 * @param {function()=} opt_callback Callback to call after the copy has
 *     completed.
 */
function copyEntryToClipboard(path, opt_callback) {
  var appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          COMPLEX_DRIVE_ENTRY_SET);
    },
    // Select the file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, [path], this.next);
    },
    // Wait for the file to be selected.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '.table-row[selected]').then(this.next);
    },
    // Right-click on the file.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the 'copy' command to appear.
    function() {
      remoteCall
          .waitForElement(
              appId, '[command="#copy"]:not([hidden]):not([disabled])')
          .then(this.next);
    },
    // Select 'copy'.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId,
          ['[command="#copy"]:not([hidden]):not([disabled])'], this.next);
    },
    // Wait for the context menu to disappear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#file-context-menu[hidden]')
          .then(this.next);
    },
    // Check for Javascript errors.
    function() {
      checkIfNoErrorsOccured(this.next);
      if (opt_callback)
        opt_callback();
    }
  ]);
}

/**
 * Tests that the Delete menu item is enabled if a read-write entry is selected.
 */
testcase.checkDeleteEnabledForReadWriteFile = function() {
  checkContextMenu('delete', 'hello.txt', true);
};

/**
 * Tests that the Delete menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyDocument = function() {
  checkContextMenu('delete', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only file is selected.
 */
testcase.checkDeleteDisabledForReadOnlyFile = function() {
  checkContextMenu('delete', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkDeleteDisabledForReadOnlyFolder = function() {
  checkContextMenu('delete', 'Read-Only Folder', false);
};

/**
 * Tests that the Rename menu item is enabled if a read-write entry is selected.
 */
testcase.checkRenameEnabledForReadWriteFile = function() {
  checkContextMenu('rename', 'hello.txt', true);
};

/**
 * Tests that the Rename menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyDocument = function() {
  checkContextMenu('rename', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only file is selected.
 */
testcase.checkRenameDisabledForReadOnlyFile = function() {
  checkContextMenu('rename', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Rename menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkRenameDisabledForReadOnlyFolder = function() {
  checkContextMenu('rename', 'Read-Only Folder', false);
};

/**
 * Tests that the Share menu item is enabled if a read-write entry is selected.
 */
testcase.checkShareEnabledForReadWriteFile = function() {
  checkContextMenu('share', 'hello.txt', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyDocument = function() {
  checkContextMenu('share', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Share menu item is disabled if a strict read-only document is
 * selected.
 */
testcase.checkShareDisabledForStrictReadOnlyDocument = function() {
  checkContextMenu('share', 'Read-Only (Strict) Doc.gdoc', false);
};

/**
 * Tests that the Share menu item is enabled if a read-only file is selected.
 */
testcase.checkShareEnabledForReadOnlyFile = function() {
  checkContextMenu('share', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Share menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkShareEnabledForReadOnlyFolder = function() {
  checkContextMenu('share', 'Read-Only Folder', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-write entry is selected.
 */
testcase.checkCopyEnabledForReadWriteFile = function() {
  checkContextMenu('copy', 'hello.txt', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only document is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyDocument = function() {
  checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is disabled if a strict (no-copy) read-only
 * document is selected.
 */
testcase.checkCopyDisabledForStrictReadOnlyDocument = function() {
  checkContextMenu('copy', 'Read-Only Doc.gdoc', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only file is selected.
 */
testcase.checkCopyEnabledForReadOnlyFile = function() {
  checkContextMenu('copy', 'Read-Only File.jpg', true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected.
 */
testcase.checkCopyEnabledForReadOnlyFolder = function() {
  checkContextMenu('copy', 'Read-Only Folder', true);
};

/**
 * Tests that the Cut menu item is enabled if a read-write entry is selected.
 */
testcase.checkCutEnabledForReadWriteFile = function() {
  checkContextMenu('cut', 'hello.txt', true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only document is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyDocument = function() {
  checkContextMenu('cut', 'Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only file is selected.
 */
testcase.checkCutDisabledForReadOnlyFile = function() {
  checkContextMenu('cut', 'Read-Only File.jpg', false);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkCutDisabledForReadOnlyFolder = function() {
  checkContextMenu('cut', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste into Folder menu item is enabled if a read-write folder
 * is selected.
 * TODO(sashab): Make this test only open one window, instead of two.
 */
testcase.checkPasteIntoFolderEnabledForReadWriteFolder = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenu('paste-into-folder', 'photos', true);
  });
};

/**
 * Tests that the Paste into Folder menu item is disabled if a read-only folder
 * is selected.
 * TODO(sashab): Make this test only open one window, instead of two.
 */
testcase.checkPasteIntoFolderDisabledForReadOnlyFolder = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenu('paste-into-folder', 'Read-Only Folder', false);
  });
};

/**
 * Tests that the New Folder menu item is enabled if a read-write folder is
 * selected.
 */
testcase.checkNewFolderEnabledForReadWriteFolder = function() {
  checkContextMenu('new-folder', 'photos', true);
};

/**
 * Tests that the New Folder menu item is disabled if a read-only folder is
 * selected.
 */
testcase.checkNewFolderDisabledForReadOnlyFolder = function() {
  checkContextMenu('new-folder', 'Read-Only Folder', false);
};

/**
 * Tests that text selection context menus are disabled in tablet mode.
 */
testcase.checkContextMenusForInputElements = function() {
  let appId;
  StepsRunner.run([
    // Start FilesApp.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS).then(this.next);
    },
    // Query all input elements.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'queryAllElements', appId,
          ['input[type=text], input[type=search], textarea, cr-input'],
          this.next);
    },
    // Focus the search box.
    function(elements) {
      chrome.test.assertEq(2, elements.length);
      for (let element of elements) {
        chrome.test.assertEq(
            '#text-context-menu', element.attributes.contextmenu);
      }

      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'focus'], this.next);
    },
    // Input a text.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'inputText', appId, ['#search-box cr-input', 'test.pdf'], this.next);
    },
    // Notify the element of the input.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'input'], this.next);
    },
    // Do the touch.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'fakeTouchClick', appId, ['#search-box cr-input'], this.next);
    },
    // Context menu must be hidden if touch induced.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#text-context-menu[hidden]')
          .then(this.next);
    },
    // Do the right click.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['#search-box cr-input'], this.next);
    },
    // Context menu must be visible if mouse induced.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#text-context-menu:not([hidden])')
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/** TODO(sashab): Add tests for copying to/from the directory tree on the LHS.
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
function checkContextMenuInDriveFolder(
    commandId, folderName, expectedEnabledState) {
  let appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          COMPLEX_DRIVE_ENTRY_SET);
    },
    // Select 'My Drive'.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'selectFolderInTree', appId, ['My Drive'], this.next);
    },
    // Expand 'My Drive'.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'expandSelectedFolderInTree', appId, [], this.next);
    },
    // Select the folder.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'selectFolderInTree', appId, [folderName], this.next);
    },
    // Right-click inside the file list.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['#file-list'], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the command option to appear.
    function() {
      let query;
      if (expectedEnabledState) {
        query = `[command="#${commandId}"]:not([hidden]):not([disabled])`;
      } else {
        query = `[command="#${commandId}"][disabled]:not([hidden])`;
      }
      remoteCall.waitForElement(appId, query).then(this.next);
    },
    // Check for Javascript errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests that the New Folder menu item is enabled inside a folder that has
 * read-write permissions.
 */
testcase.checkNewFolderEnabledInsideReadWriteFolder = function() {
  checkContextMenuInDriveFolder('new-folder', 'photos', true);
};

/**
 * Tests that the New Folder menu item is enabled inside a folder that has
 * read-write permissions.
 */
testcase.checkNewFolderDisabledInsideReadOnlyFolder = function() {
  checkContextMenuInDriveFolder('new-folder', 'Read-Only Folder', false);
};

/**
 * Tests that the Paste menu item is enabled inside a folder that has read-write
 * permissions.
 * TODO(sashab): Make this test only open one window, instead of two.
 */
testcase.checkPasteEnabledInsideReadWriteFolder = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenuInDriveFolder('paste', 'photos', true);
  });
};

/**
 * Tests that the Paste menu item is disabled inside a folder that has read-only
 * permissions.
 * TODO(sashab): Make this test only open one window, instead of two.
 */
testcase.checkPasteDisabledInsideReadOnlyFolder = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenuInDriveFolder('paste', 'Read-Only Folder', false);
  });
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
function checkContextMenuForDriveFolderInTree(
    commandId, folderSelector, expectedEnabledState) {
  let appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          COMPLEX_DRIVE_ENTRY_SET);
    },
    // Set focus on the file list.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'focus', appId, ['#file-list:not([hidden])'], this.next);
    },
    // Select 'My Drive'.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'selectFolderInTree', appId, ['My Drive'], this.next);
    },
    // Expand 'My Drive'.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'expandSelectedFolderInTree', appId, [], this.next);
    },
    // Wait for the folder to be visible.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, `${folderSelector}:not([hidden])`)
          .then(this.next);
    },
    // Focus the selected folder.
    function() {
      remoteCall.callRemoteTestUtil(
          'focus', appId, ['#directory-tree'], this.next);
    },
    // Right-click the selected folder.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId,
          [`${folderSelector}:not([hidden]) .label`], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall
          .waitForElement(appId, '#directory-tree-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the command option to appear.
    function() {
      let query;
      if (expectedEnabledState) {
        query = `[command="#${commandId}"]:not([hidden]):not([disabled])`;
      } else {
        query = `[command="#${commandId}"][disabled]:not([hidden])`;
      }
      remoteCall.waitForElement(appId, query).then(this.next);
    },
    // Check for Javascript errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests that the Copy menu item is enabled if a read-write folder is selected
 * in the directory tree.
 */
testcase.checkCopyEnabledForReadWriteFolderInTree = function() {
  checkContextMenuForDriveFolderInTree(
      'copy',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Copy menu item is enabled if a read-only folder is
 * selected in the directory tree.
 */
testcase.checkCopyEnabledForReadOnlyFolderInTree = function() {
  checkContextMenuForDriveFolderInTree(
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
  checkContextMenuForDriveFolderInTree(
      'cut',
      '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
      true);
};

/**
 * Tests that the Cut menu item is disabled if a read-only folder is
 * selected in the directory tree.
 */
testcase.checkCutDisabledForReadOnlyFolderInTree = function() {
  checkContextMenuForDriveFolderInTree(
      'cut',
      '#directory-tree [full-path-for-testing="/root/Read-Only Folder"]' +
          ':not([hidden])',
      false);
};

/**
 * Tests that the Paste menu item is enabled in the directory
 * tree for a folder that has read-write permissions.
 * TODO(sashab): Make this test only open one window, instead of two.
 */
testcase.checkPasteEnabledForReadWriteFolderInTree = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenuForDriveFolderInTree(
        'paste-into-folder',
        '#directory-tree [full-path-for-testing="/root/photos"]:not([hidden])',
        true);
  });
};

/**
 * Tests that the Paste menu item is disabled in the directory tree for a folder
 * that has read-only permissions.
 * TODO(sashab): Make this test only open one window, instead of two.
 */
testcase.checkPasteDisabledForReadOnlyFolderInTree = function() {
  copyEntryToClipboard('hello.txt', () => {
    checkContextMenuForDriveFolderInTree(
        'paste-into-folder',
        '#directory-tree [full-path-for-testing="/root/Read-Only Folder"]' +
            ':not([hidden])',
        false);
  });
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
function checkTeamDriveContextMenuInTree(
    teamDriveName, expectedContextMenuState) {
  let navItemSelector = `#directory-tree ` +
      `.tree-item[full-path-for-testing="/team_drives/${teamDriveName}"]`;
  let appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          TEAM_DRIVE_ENTRY_SET);
    },
    // Set focus on the file list.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'focus', appId, ['#file-list:not([hidden])'], this.next);
    },
    // Select 'Team Drives'.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'selectFolderInTree', appId, ['Team Drives'], this.next);
    },
    // Expand 'Team Drives'.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'expandSelectedFolderInTree', appId, [], this.next);
    },
    // Wait for the team drive to be visible.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, `${navItemSelector}:not([hidden])`)
          .then(this.next);
    },
    // Focus the selected team drive.
    function() {
      remoteCall.callRemoteTestUtil(
          'focus', appId, ['#directory-tree'], this.next);
    },
    // Right-click the selected team drive.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId,
          [`${navItemSelector}:not([hidden]) .label`], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall
          .waitForElement(appId, '#directory-tree-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the command options to appear.
    function() {
      let query;
      let promises = [];
      for (let commandId in expectedContextMenuState) {
        if (expectedContextMenuState[commandId] == true) {
          query = `[command="#${commandId}"]:not([hidden]):not([disabled])`;
        } else {
          query = `[command="#${commandId}"][disabled]:not([hidden])`;
        }
        promises.push(remoteCall.waitForElement(appId, query));
      }
      Promise.all(promises).then(this.next);
    },
    // Check for Javascript errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests that the context menu contains the correct items with the correct
 * enabled/disabled state if a Team Drive Root is selected.
 */
testcase.checkContextMenuForTeamDriveRoot = function() {
  checkTeamDriveContextMenuInTree('Team Drive A', {
    'cut': false,
    'copy': false,
    'rename': false,
    'delete': false,
    'new-folder': true
  });
};
