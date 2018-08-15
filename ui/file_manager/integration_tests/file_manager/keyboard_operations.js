// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Waits until a dialog with an OK button is shown, and accepts it by clicking
 * on the dialog's OK button.
 *
 * @param {string} windowId Window ID.
 * @return {Promise} Promise to be fulfilled after clicking the OK button.
 */
function waitAndAcceptDialog(windowId) {
  const dialogButton = '.cr-dialog-ok';
  return remoteCall.waitForElement(windowId, dialogButton)
      .then(function() {
        return remoteCall.callRemoteTestUtil(
            'fakeMouseClick', windowId, [dialogButton]);
      })
      .then(function(result) {
        chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
        const dialog = '.cr-dialog-container';
        return remoteCall.waitForElementLost(windowId, dialog);
      });
}

/**
 * Returns the visible directory tree items.
 *
 * @param {string} windowId Window ID.
 * @return {!Promise<!Array<string>>} List of visible item names.
 */
function getTreeItems(windowId) {
  return remoteCall.callRemoteTestUtil('getTreeItems', windowId, []);
}

/**
 * Waits until the directory tree item |name| appears.
 *
 * @param {string} windowId Window ID.
 * @param {string} name Name of tree item.
 * @return {!Promise}
 */
function waitForDirectoryItem(windowId, name) {
  var caller = getCaller();
  return repeatUntil(function() {
    return getTreeItems(windowId).then(function(items) {
      if (items.indexOf(name) !== -1) {
        return true;
      } else {
        return pending(caller, 'Tree item %s is not found.', name);
      }
    });
  });
}

/**
 * Waits until the directory tree item |name| disappears.
 *
 * @param {string} windowId Window ID.
 * @param {string} name Name of tree item.
 * @return {!Promise}
 */
function waitForDirectoryItemLost(windowId, name) {
  var caller = getCaller();
  return repeatUntil(function() {
    return getTreeItems(windowId).then(function(items) {
      console.log(items);
      if (items.indexOf(name) === -1) {
        return true;
      } else {
        return pending(caller, 'Tree item %s is still exists.', name);
      }
    });
  });
}

/**
 * Tests copying a file to the same volume path file list.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 */
function keyboardCopy(path, callback) {
  let fileListBefore;
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
      fileListBefore = results.fileList;
      remoteCall.callRemoteTestUtil(
          'copyFile', appId, ['world.ogv'], this.next);
    },
    // Check: the copied file should appear in the file list.
    function(result) {
      chrome.test.assertTrue(!!result, 'copyFile failed');
      const expectedFilesAfter =
          fileListBefore.concat([['world (1).ogv', '59 KB', 'OGG video']]);
      remoteCall
          .waitForFiles(
              appId, expectedFilesAfter, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests deleting file entry from the file list.
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
 * Tests deleting an directory entry from the file list. The directory entry is
 * also shown in the Files app directory tree, and should not be shown there
 * when the directory entry is deleted.
 *
 * @param {string} path The path to be tested, Downloads or Drive.
 * @param {string} treeItem The directory tree item selector.
 */
function keyboardDeleteDirectory(path, treeItem) {
  let appId;

  StepsRunner.run([
    // Open Files app on |path| containing one directory entry: photos.
    function() {
      setupAndWaitUntilReady(
          null, path, this.next, [ENTRIES.photos], [ENTRIES.photos]);
    },
    // Expand the directory tree |treeItem|.
    function(results) {
      appId = results.windowId;
      expandRoot(appId, treeItem).then(this.next);
    },
    // Check: the directory entry should be shown in the directory tree.
    function() {
      waitForDirectoryItem(appId, 'photos').then(this.next);
    },
    // Delete the directory entry from the file list.
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
    // Check: the directory entry should not be shown in the directory tree.
    function() {
      waitForDirectoryItemLost(appId, 'photos').then(this.next);
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
 * Test for renaming a new directory.
 * @param {string} path Initial path.
 * @param {Array<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @param {string} pathInBreadcrumb Initial path which is shown in breadcrumb.
 * @return {Promise} Promise to be fulfilled on success.
 */
function testRenameNewDirectory(path, initialEntrySet, pathInBreadcrumb) {
  var expectedRows = TestEntryInfo.getExpectedRows(initialEntrySet);

  return new Promise(function(resolve) {
    setupAndWaitUntilReady(null, path, resolve);
  }).then(function(results) {
    var windowId = results.windowId;
    return remoteCall.waitForFiles(windowId, expectedRows).then(function() {
      return remoteCall.fakeKeyDown(
          windowId, '#list-container', 'e', 'U+0045', true, false, false);
    }).then(function() {
      // Wait for rename text field.
      return remoteCall.waitForElement(windowId, 'input.rename');
    }).then(function() {
      // Type new file name.
      return remoteCall.callRemoteTestUtil(
          'inputText', windowId, ['input.rename', 'foo']);
    }).then(function() {
      // Press Enter.
      return remoteCall.fakeKeyDown(
          windowId, 'input.rename', 'Enter', 'Enter', false, false, false);
    }).then(function() {
      // Press Enter again to try to get into the new directory.
      return remoteCall.fakeKeyDown(
          windowId, '#list-container', 'Enter', 'Enter', false, false, false);
    }).then(function() {
      // Confirm that it doesn't move the directory since it's in renaming
      // process.
      return remoteCall.waitUntilCurrentDirectoryIsChanged(windowId,
          pathInBreadcrumb);
    }).then(function() {
      // Wait until rename is completed.
      return remoteCall.waitForElementLost(windowId, 'li[renaming]');
    }).then(function() {
      // Press Enter again.
      return remoteCall.fakeKeyDown(windowId, '#list-container', 'Enter',
          'Enter', false, false, false);
    }).then(function() {
      // Confirm that it moves to renamed directory.
      return remoteCall.waitUntilCurrentDirectoryIsChanged(windowId,
          pathInBreadcrumb + '/foo');
    });
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
  keyboardDeleteDirectory(RootPath.DOWNLOADS, TREEITEM_DOWNLOADS);
};

testcase.keyboardDeleteFolderDrive = function() {
  keyboardDeleteDirectory(RootPath.DRIVE, TREEITEM_DRIVE);
};

testcase.renameFileDownloads = function() {
  testPromise(testRenameFile(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET));
};

testcase.renameFileDrive = function() {
  testPromise(testRenameFile(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET));
};

testcase.renameNewFolderDownloads = function() {
  testPromise(testRenameNewDirectory(RootPath.DOWNLOADS,
      BASIC_LOCAL_ENTRY_SET, '/Downloads'));
};

testcase.renameNewFolderDrive = function() {
  testPromise(testRenameNewDirectory(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET,
      '/My Drive'));
};
