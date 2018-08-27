// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Waits until a dialog with an OK button is shown, and accepts it by clicking
 * on the dialog's OK button.
 *
 * @param {string} appId The Files app windowId.
 * @return {Promise} Promise to be fulfilled after clicking the OK button.
 */
function waitAndAcceptDialog(appId) {
  const okButton = '.cr-dialog-ok';
  return new Promise(function(resolve) {
    // Wait for the Ok button to appear.
    remoteCall.waitForElement(appId, okButton).then(resolve);
  }).then(function() {
    // Click the Ok button.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [okButton]);
  }).then(function(result) {
    chrome.test.assertTrue(result, 'Dialog Ok button click failed');
    // Wait until the dialog closes.
    return remoteCall.waitForElementLost(appId, '.cr-dialog-container');
  });
}

/**
 * Returns the visible directory tree item names.
 *
 * @param {string} appId The Files app windowId.
 * @return {!Promise<!Array<string>>} List of visible item names.
 */
function getVisibleDirectoryTreeItemNames(appId) {
  return remoteCall.callRemoteTestUtil('getTreeItems', appId, []);
}

/**
 * Waits until the directory tree item |name| appears.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} name Directory tree item name.
 * @return {!Promise}
 */
function waitForDirectoryTreeItem(appId, name) {
  let caller = getCaller();
  return repeatUntil(function() {
    return getVisibleDirectoryTreeItemNames(appId).then(function(names) {
      if (names.indexOf(name) !== -1) {
        return true;
      } else {
        return pending(caller, 'Directory tree item %s not found.', name);
      }
    });
  });
}

/**
 * Waits until the directory tree item |name| disappears.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} name Directory tree item name.
 * @return {!Promise}
 */
function waitForDirectoryTreeItemLost(appId, name) {
  let caller = getCaller();
  return repeatUntil(function() {
    return getVisibleDirectoryTreeItemNames(appId).then(function(names) {
      if (names.indexOf(name) === -1) {
        return true;
      } else {
        return pending(caller, 'Directory tree item %s still exists.', name);
      }
    });
  });
}

/**
 * Tests copying a file to the same file list.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 */
function keyboardCopy(path) {
  let appId;

  StepsRunner.run([
    // Open Files app on |path| containing one file entry: world.
    function() {
      setupAndWaitUntilReady(
          null, path, this.next, [ENTRIES.world], [ENTRIES.world]);
    },
    // Copy the file into the same file list.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'copyFile', appId, ['world.ogv'], this.next);
    },
    // Check: the copied file should appear in the file list.
    function(result) {
      chrome.test.assertTrue(result, 'copyFile failed');
      const files = [['world.ogv', '59 KB', 'OGG video']].concat(
          [['world (1).ogv', '59 KB', 'OGG video']]);
      remoteCall.waitForFiles(
          appId, files, {ignoreLastModifiedTime: true}).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests deleting a file from the file list.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 */
function keyboardDelete(path) {
  let appId;

  StepsRunner.run([
    // Open Files app on |path| containing one file entry: hello.
    function() {
      setupAndWaitUntilReady(
          null, path, this.next, [ENTRIES.hello], [ENTRIES.hello]);
    },
    // Delete the file from the file list.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'deleteFile', appId, ['hello.txt'], this.next);
    },
    // Run the delete entry confirmation dialog.
    function(result) {
      chrome.test.assertTrue(result, 'deleteFile failed');
      waitAndAcceptDialog(appId).then(this.next);
    },
    // Check: the file list should now be empty.
    function() {
      remoteCall.waitForFiles(appId, []).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests deleting a folder from the file list. The folder is also shown in the
 * Files app directory tree, and should not be shown there when deleted.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 * @param {string} treeItem The directory tree item selector.
 */
function keyboardDeleteFolder(path, treeItem) {
  let appId;

  StepsRunner.run([
    // Open Files app on |path| containing one folder entry: photos.
    function() {
      setupAndWaitUntilReady(
          null, path, this.next, [ENTRIES.photos], [ENTRIES.photos]);
    },
    // Expand the directory tree |treeItem|.
    function(results) {
      appId = results.windowId;
      expandRoot(appId, treeItem).then(this.next);
    },
    // Check: the folder should be shown in the directory tree.
    function() {
      waitForDirectoryTreeItem(appId, 'photos').then(this.next);
    },
    // Delete the folder entry from the file list.
    function() {
      remoteCall.callRemoteTestUtil('deleteFile', appId, ['photos'], this.next);
    },
    // Run the delete entry confirmation dialog.
    function(result) {
      chrome.test.assertTrue(result, 'deleteFile failed');
      waitAndAcceptDialog(appId).then(this.next);
    },
    // Check: the file list should now be empty.
    function() {
      remoteCall.waitForFiles(appId, []).then(this.next);
    },
    // Check: the folder should not be shown in the directory tree.
    function() {
      waitForDirectoryTreeItemLost(appId, 'photos').then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Renames a file.
 * @param {string} windowId ID of the window.
 * @param {string} oldName Old name of a file.
 * @param {string} newName New name of a file.
 * @return {Promise} Promise to be fulfilled on success.
 */
function renameFile(windowId, oldName, newName) {
  return remoteCall.callRemoteTestUtil('selectFile', windowId, [oldName]).
    then(function() {
      // Push Ctrl+Enter.
      return remoteCall.fakeKeyDown(
          windowId, '#detail-table', 'Enter', 'Enter', true, false, false);
    }).then(function() {
      // Wait for rename text field.
      return remoteCall.waitForElement(windowId, 'input.rename');
    }).then(function() {
      // Type new file name.
      return remoteCall.callRemoteTestUtil(
          'inputText', windowId, ['input.rename', newName]);
    }).then(function() {
      // Push Enter.
      return remoteCall.fakeKeyDown(
          windowId, 'input.rename', 'Enter', 'Enter', false, false, false);
    });
}

/**
 * Tests renaming a folder. An extra enter key is sent to the file list during
 * renaming to check the folder cannot be entered while it is being renamed.
 *
 * @param {string} path Initial path (Downloads or Drive).
 * @param {string} treeItem The directory tree item selector.
 * @return {Promise} Promise to be fulfilled on success.
 */
function testRenameFolder(path, treeItem) {
  let windowId;

  const textInput = '#file-list .table-row[renaming] input.rename';

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(
        null, path, resolve, [ENTRIES.photos], [ENTRIES.photos]);
  }).then(function(results) {
    // Expand the directory tree |treeItem|.
    windowId = results.windowId;
    return expandRoot(windowId, treeItem);
  }).then(function() {
    // Check: the photos folder should be shown in the directory tree.
    return waitForDirectoryTreeItem(windowId, 'photos');
  }).then(function() {
    // Focus the file-list.
    return remoteCall.callRemoteTestUtil('focus', windowId, ['#file-list']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Press ArrowDown to select the photos folder.
    const select = ['#file-list', 'ArrowDown', 'Down', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId, select);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Await file list item selection.
    const selectedItem = '#file-list .table-row[selected]';
    return remoteCall.waitForElement(windowId, selectedItem);
  }).then(function() {
    // Press Ctrl+Enter to rename the photos folder.
    const key = ['#file-list', 'Enter', 'Enter', true, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Check: the renaming text input should be shown in the file list.
    return remoteCall.waitForElement(windowId, textInput);
  }).then(function() {
    // Type the new folder name.
    return remoteCall.callRemoteTestUtil(
        'inputText', windowId, [textInput, 'bbq photos']);
  }).then(function() {
    // Send Enter to the list to attempt to enter the directory.
    const key = ['#list-container', 'Enter', 'Enter', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Send Enter to the text input to complete renaming.
    const key = [textInput, 'Enter', 'Enter', false, false, false];
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId, key);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    // Wait until renaming is complete.
    const renamingItem = '#file-list .table-row[renaming]';
    return remoteCall.waitForElementLost(windowId, renamingItem);
  }).then(function() {
    // Check: the renamed folder should be shown in the file list.
    const expectedRows = [['bbq photos', '--', 'Folder', '']];
    return remoteCall.waitForFiles(
        windowId, expectedRows, {ignoreLastModifiedTime: true});
  }).then(function() {
    // Check: the renamed folder should be shown in the directory tree.
    return waitForDirectoryTreeItem(windowId, 'bbq photos');
  });
}

/**
 * Test for renaming a file.
 * @param {string} path Initial path.
 * @param {Array<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @return {Promise} Promise to be fulfilled on success.
 */
function testRenameFile(path, initialEntrySet) {
  var windowId;

  // Make expected rows.
  var initialExpectedEntryRows = TestEntryInfo.getExpectedRows(initialEntrySet);
  var expectedEntryRows = TestEntryInfo.getExpectedRows(initialEntrySet);
  for (var i = 0; i < expectedEntryRows.length; i++) {
    if (expectedEntryRows[i][0] === 'hello.txt') {
      expectedEntryRows[i][0] = 'New File Name.txt';
      break;
    }
  }
  chrome.test.assertTrue(
      i != expectedEntryRows.length, 'hello.txt is not found.');

  // Open a window.
  return new Promise(function(callback) {
    setupAndWaitUntilReady(null, path, callback);
  }).then(function(results) {
    windowId = results.windowId;
    return remoteCall.waitForFiles(windowId, initialExpectedEntryRows);
  }).then(function(){
    return renameFile(windowId, 'hello.txt', 'New File Name.txt');
  }).then(function() {
    // Wait until rename completes.
    return remoteCall.waitForElementLost(windowId, '#detail-table [renaming]');
  }).then(function() {
    // Wait for the new file name.
    return remoteCall.waitForFiles(windowId,
                        expectedEntryRows,
                        {ignoreLastModifiedTime: true});
  }).then(function() {
    return renameFile(windowId, 'New File Name.txt', '.hidden file');
  }).then(function() {
    // The error dialog is shown.
    return waitAndAcceptDialog(windowId);
  }).then(function() {
    // The name did not change.
    return remoteCall.waitForFiles(windowId,
                        expectedEntryRows,
                        {ignoreLastModifiedTime: true});
  });
}

testcase.keyboardCopyDownloads = function() {
  keyboardCopy(RootPath.DOWNLOADS);
};

testcase.keyboardCopyDrive = function() {
  keyboardCopy(RootPath.DRIVE);
};

testcase.keyboardDeleteDownloads = function() {
  keyboardDelete(RootPath.DOWNLOADS);
};

testcase.keyboardDeleteDrive = function() {
  keyboardDelete(RootPath.DRIVE);
};

testcase.keyboardDeleteFolderDownloads = function() {
  keyboardDeleteFolder(RootPath.DOWNLOADS, TREEITEM_DOWNLOADS);
};

testcase.keyboardDeleteFolderDrive = function() {
  keyboardDeleteFolder(RootPath.DRIVE, TREEITEM_DRIVE);
};

testcase.renameFileDownloads = function() {
  testPromise(testRenameFile(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET));
};

testcase.renameFileDrive = function() {
  testPromise(testRenameFile(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET));
};

testcase.renameNewFolderDownloads = function() {
  testPromise(testRenameFolder(RootPath.DOWNLOADS, TREEITEM_DOWNLOADS));
};

testcase.renameNewFolderDrive = function() {
  testPromise(testRenameFolder(RootPath.DRIVE, TREEITEM_DRIVE));
};
