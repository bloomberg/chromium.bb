// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sets up for directory tree context menu test. In addition to normal setup, we
 * add destination directory.
 */
function setupForDirectoryTreeContextMenuTest() {
  var windowId;
  return setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS).then(function(results) {
    windowId = results.windowId;

    // Add destination directory.
    return new addEntries(['local'], [new TestEntryInfo({
                            type: EntryType.DIRECTORY,
                            targetPath: 'destination',
                            lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                            nameText: 'destination',
                            sizeText: '--',
                            typeText: 'Folder'
                          })]);
  }).then(function() {
    return windowId;
  });
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
function clickDirectoryTreeContextMenuItem(windowId, path, id) {
  const contextMenu = '#directory-tree-context-menu:not([hidden])';
  const pathQuery = `#directory-tree [full-path-for-testing="${path}"]`;

  return remoteCall.callRemoteTestUtil('focus', windowId,
      [pathQuery]).then(function(result) {
    chrome.test.assertTrue(!!result, 'focus failed');
    // Right click photos directory.
    return remoteCall.callRemoteTestUtil('fakeMouseRightClick', windowId,
            [pathQuery]);
  }).then(function(result) {
    chrome.test.assertTrue(!!result, 'fakeMouseRightClick failed');
    // Check: context menu item |id| should be shown enabled.
    return remoteCall.waitForElement(windowId,
        `${contextMenu} [command="#${id}"]:not([disabled])`);
  }).then(function() {
    // Click the menu item specified by |id|.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        [`${contextMenu} [command="#${id}"]`]);
  });
}

/**
 * Navigates to destination directory and test paste operation to check whether
 * the paste operation is done correctly or not. This method does NOT check
 * source entry is deleted or not for cut operation.
 */
function navigateToDestinationDirectoryAndTestPaste(windowId) {
  // Navigates to destination directory.
  return remoteCall
      .navigateWithDirectoryTree(
          windowId, RootPath.DOWNLOADS_PATH + '/destination',
          'My files/Downloads')
      .then(function() {
        // Confirm files before paste.
        return remoteCall.waitForFiles(
            windowId, ITEMS_IN_DEST_DIR_BEFORE_PASTE,
            {ignoreLastModifiedTime: true});
      })
      .then(function() {
        // Paste
        return remoteCall.callRemoteTestUtil(
            'fakeKeyDown', windowId,
            ['body', 'v', true /* ctrl */, false, false]);
      })
      .then(function() {
        // Confirm the photos directory is pasted correctly.
        return remoteCall.waitForFiles(
            windowId, ITEMS_IN_DEST_DIR_AFTER_PASTE,
            {ignoreLastModifiedTime: true});
      });
}

/**
 * Rename photos directory to specified name by using directory tree.
 */
function renamePhotosDirectoryTo(windowId, newName, useKeyboardShortcut) {
  return (useKeyboardShortcut ?
      remoteCall.callRemoteTestUtil(
          'fakeKeyDown', windowId,
          ['body', 'Enter', true /* ctrl */, false, false]) :
      clickDirectoryTreeContextMenuItem(
          windowId, RootPath.DOWNLOADS_PATH + '/photos', 'rename')
      ).then(function() {
    return remoteCall.waitForElement(windowId, '.tree-row > input');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'inputText', windowId, ['.tree-row > input', newName]);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeKeyDown', windowId,
        ['.tree-row > input', 'Enter', false, false, false]);
  });
}

/**
 * Renames directory and confirm current directory is moved to the renamed
 * directory.
 */
function renameDirectoryFromDirectoryTreeSuccessCase(useKeyboardShortcut) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  }).then(function() {
    return renamePhotosDirectoryTo(windowId, 'New photos', useKeyboardShortcut);
  }).then(function() {
    // Confirm that current directory has moved to new folder.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId, '/My files/Downloads/New photos');
  });
}

/**
 * Renames directory and confirms that an alert dialog is shown.
 */
function renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(newName) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  }).then(function() {
    return renamePhotosDirectoryTo(windowId, newName, false);
  }).then(function() {
    // Confirm that a dialog is shown.
    return remoteCall.waitForElement(windowId, '.cr-dialog-container.shown');
  });
}

/**
 * Creates directory from directory tree.
 */
function createDirectoryFromDirectoryTree(
    useKeyboardShortcut, changeCurrentDirectory) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;

    if (changeCurrentDirectory) {
      return remoteCall.navigateWithDirectoryTree(
          windowId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
    } else {
      const downloadsQuery =
          '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
      return remoteCall.expandDirectoryTreeFor(windowId, downloadsQuery);
    }
  }).then(function() {
    if (useKeyboardShortcut) {
      return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
          ['body', 'e', true /* ctrl */, false, false]);
    } else {
      return clickDirectoryTreeContextMenuItem(
          windowId, RootPath.DOWNLOADS_PATH +'/photos', 'new-folder');
    }
  }).then(function() {
    return remoteCall.waitForElement(windowId, '.tree-row > input');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'inputText', windowId, ['.tree-row > input', 'test']);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeKeyDown', windowId,
        ['.tree-row > input', 'Enter', false, false, false]);
  }).then(function() {
    // Confirm that new directory is added to the directory tree.
    return remoteCall.waitForElement(
        windowId,
        `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos/test"]`);
  }).then(function() {
    // Confirm that current directory is not changed at this timing.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId,
        changeCurrentDirectory ? '/My files/Downloads/photos' :
                                 '/My files/Downloads');
  }).then(function() {
    // Confirm that new directory is actually created by navigating to it.
    return remoteCall.navigateWithDirectoryTree(
        windowId, RootPath.DOWNLOADS_PATH + '/photos/test',
        'My files/Downloads');
  });
}

/**
 * Tests copying a directory from directory tree with context menu.
 */
testcase.dirCopyWithContextMenu = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'copy');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Tests copying a directory from directory tree with the keyboard shortcut.
 */
testcase.dirCopyWithKeyboard = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  }).then(function() {
    // Press Ctrl+C.
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'c', true /* ctrl */, false, false]);
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Tests copying a directory without changing the current directory.
 */
testcase.dirCopyWithoutChangingCurrent = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    const downloadsQuery =
        '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
    return remoteCall.expandTreeItemInDirectoryTree(windowId, downloadsQuery);
  }).then(function(result) {
    return clickDirectoryTreeContextMenuItem(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'copy');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Tests cutting a directory with the context menu.
 */
testcase.dirCutWithContextMenu = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId,  RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'cut');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
    // Confirm that directory tree is updated.
    return remoteCall.waitForElementLost(
        windowId,
        `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
  }));
};

/**
 * Tests cutting a directory with the keyboard shortcut.
 */
testcase.dirCutWithKeyboard = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.navigateWithDirectoryTree(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');
  }).then(function() {
    // Press Ctrl+X.
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'x', true /* ctrl */, false, false]);
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
     // Confirm that directory tree is updated.
    return remoteCall.waitForElementLost(
        windowId,
        `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
  }));
};

/**
 * Tests cutting a directory without changing the current directory.
 */
testcase.dirCutWithoutChangingCurrent = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    const downloadsQuery =
        '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
    return remoteCall.expandTreeItemInDirectoryTree(windowId, downloadsQuery);
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'cut');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
    return remoteCall.waitForElementLost(
        windowId,
        `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
  }));
};

/**
 * Tests pasting into folder with the context menu.
 */
testcase.dirPasteWithContextMenu = function() {
  var windowId;
  const destinationPath = RootPath.DOWNLOADS_PATH + '/destination';
  testPromise(
      setupForDirectoryTreeContextMenuTest()
          .then(function(id) {
            // Copy photos directory as a test data.
            windowId = id;
            return remoteCall.navigateWithDirectoryTree(
                windowId, RootPath.DOWNLOADS_PATH + '/photos',
                'My files/Downloads');
          })
          .then(function() {
            return remoteCall.callRemoteTestUtil(
                'fakeKeyDown', windowId,
                ['body', 'c', true /* ctrl */, false, false]);
          })
          .then(function() {
            return remoteCall.navigateWithDirectoryTree(
                windowId, RootPath.DOWNLOADS_PATH + '/destination',
                'My files/Downloads');
          })
          .then(function() {
            // Confirm files before paste.
            return remoteCall.waitForFiles(
                windowId, ITEMS_IN_DEST_DIR_BEFORE_PASTE,
                {ignoreLastModifiedTime: true});
          })
          .then(function() {
            return clickDirectoryTreeContextMenuItem(
                windowId, RootPath.DOWNLOADS_PATH + '/destination',
                'paste-into-folder');
          })
          .then(function() {
            // Confirm the photos directory is pasted correctly.
            return remoteCall.waitForFiles(
                windowId, ITEMS_IN_DEST_DIR_AFTER_PASTE,
                {ignoreLastModifiedTime: true});
          })
          .then(function() {
            // Expand the directory tree.
            return remoteCall.waitForElement(
                windowId,
                `[full-path-for-testing="${destinationPath}"] .expand-icon`);
          })
          .then(function() {
            return remoteCall.callRemoteTestUtil(
                'fakeMouseClick', windowId,
                [`[full-path-for-testing="${destinationPath}"] .expand-icon`]);
          })
          .then(function() {
            // Confirm the copied directory is added to the directory tree.
            return remoteCall.waitForElement(
                windowId,
                `[full-path-for-testing="${destinationPath}/photos"]`);
          }));
};

/**
 * Tests pasting into a folder without changing the current directory.
 */
testcase.dirPasteWithoutChangingCurrent = function() {
  var windowId;
  const destinationPath = RootPath.DOWNLOADS_PATH + '/destination';
  const downloadsQuery =
      '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return remoteCall.expandDirectoryTreeFor(windowId, downloadsQuery);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeMouseClick', windowId,
        [`${downloadsQuery} .expand-icon`]);
  }).then((result) => {
    chrome.test.assertTrue(result, 'click on expand icon failed');
    return remoteCall.waitForElement(windowId, downloadsQuery + '[expanded]');
  }).then(() => {
    return remoteCall.callRemoteTestUtil(
        'focus', windowId, ['#directory-tree']);
  }).then(function(result) {
    return clickDirectoryTreeContextMenuItem(
        windowId, RootPath.DOWNLOADS_PATH + '/photos', 'copy');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(
        windowId, destinationPath, 'paste-into-folder');
  }).then(function() {
    return remoteCall.waitForElement(windowId,
        `[full-path-for-testing="${destinationPath}"][may-have-children]`);
  }).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        [`[full-path-for-testing="${destinationPath}"] .expand-icon`]);
  }).then(function() {
    // Confirm the copied directory is added to the directory tree.
    return remoteCall.waitForElement(windowId,
        `[full-path-for-testing="${destinationPath}/photos"]`);
  }));
};

/**
 * Tests renaming a folder with the context menu.
 */
testcase.dirRenameWithContextMenu = function() {
  testPromise(renameDirectoryFromDirectoryTreeSuccessCase(
      false /* do not use keyboard shortcut */));
};

/**
 * Tests that a child folder breadcrumbs is updated when renaming its parent
 * folder. crbug.com/885328.
 */
testcase.dirRenameUpdateChildrenBreadcrumbs = function() {
  let appId;
  testPromise(
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS)
          .then(function(results) {
            appId = results.windowId;

            // Add child-folder inside /photos/
            return new addEntries(['local'], [new TestEntryInfo({
                                    type: EntryType.DIRECTORY,
                                    targetPath: 'photos/child-folder',
                                    lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
                                    nameText: 'child-folder',
                                    sizeText: '--',
                                    typeText: 'Folder'
                                  })]);
          })
          .then(function() {
            // Navigate to child folder.
            return remoteCall.navigateWithDirectoryTree(
                appId, RootPath.DOWNLOADS_PATH + '/photos/child-folder',
                'My files/Downloads');
          })
          .then(function() {
            // Rename parent folder.
            return clickDirectoryTreeContextMenuItem(
                appId, RootPath.DOWNLOADS_PATH + '/photos', 'rename')
                .then(function() {
                  return remoteCall.waitForElement(appId, '.tree-row > input');
                })
                .then(function() {
                  return remoteCall.callRemoteTestUtil(
                      'inputText', appId, ['.tree-row > input', 'photos-new']);
                })
                .then(function() {
                  const enterKey = [
                    '.tree-row > input', 'Enter', false, false, false
                  ];
                  return remoteCall.callRemoteTestUtil(
                      'fakeKeyDown', appId, enterKey);
                })
                .then(function(result) {
                  chrome.test.assertTrue(result, 'Enter key failed');
                });
          })
          .then(function() {
            // Confirm that current directory is now My files or /Downloads,
            // because it can't find the previously selected folder
            // /Downloads/photos/child-folder, since its path/parent has been
            // renamed.
            let volumeFolder = '/My files/Downloads';
            // TODO(lucmult): Remove this conditional once MyFilesVolume is
            // rolled out.
            if (RootPath.DOWNLOADS_PATH === '/Downloads')
              volumeFolder = '/My files';

            return remoteCall.waitUntilCurrentDirectoryIsChanged(
                appId, volumeFolder);
          })
          .then(function() {
            // Navigate to child-folder using the new path.
            // |navigateWithDirectoryTree| already checks for breadcrumbs to
            // match the path.
            return remoteCall.navigateWithDirectoryTree(
                appId, RootPath.DOWNLOADS_PATH + '/photos-new/child-folder',
                'My files/Downloads');
          }));
};

/**
 * Tests renaming folder with the keyboard shortcut.
 */
testcase.dirRenameWithKeyboard = function() {
  testPromise(renameDirectoryFromDirectoryTreeSuccessCase(
      true /* use keyboard shortcut */));
};

/**
 * Tests renaming folder without changing the current directory.
 */
testcase.dirRenameWithoutChangingCurrent = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    const downloadsQuery =
        '#directory-tree [entry-label="My files"] [entry-label="Downloads"]';
    return remoteCall.expandDirectoryTreeFor(windowId, downloadsQuery);
  }).then(function() {
    return remoteCall.waitForElement(
        windowId,
        `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
  }).then(function() {
    return renamePhotosDirectoryTo(
        windowId, 'New photos', false /* Do not use keyboard shortcut. */);
  }).then(function() {
    return remoteCall.waitForElementLost(
        windowId,
        `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/photos"]`);
  }).then(function() {
    return remoteCall.waitForElement(
        windowId,
        `[full-path-for-testing="${RootPath.DOWNLOADS_PATH}/New photos"]`);
  }));
};

/**
 * Tests renaming a folder to an empty string.
 */
testcase.dirRenameToEmptyString = function() {
  testPromise(renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(''));
};

/**
 * Tests renaming folder an existing name.
 */
testcase.dirRenameToExisting = function() {
  testPromise(renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(
      'destination'));
};

/**
 * Tests creating a folder with the context menu.
 */
testcase.dirCreateWithContextMenu = function() {
  testPromise(createDirectoryFromDirectoryTree(
      false /* do not use keyboard shortcut */,
      true /* change current directory */));
};

/**
 * Tests creating a folder with the keyboard shortcut.
 */
testcase.dirCreateWithKeyboard = function() {
  testPromise(createDirectoryFromDirectoryTree(
      true /* use keyboard shortcut */,
      true /* change current directory */));
};

/**
 * Tests creating folder without changing the current directory.
 */
testcase.dirCreateWithoutChangingCurrent = function() {
  testPromise(createDirectoryFromDirectoryTree(
      false /* Do not use keyboard shortcut */,
      false /* Do not change current directory */));
};
