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
      null, RootPath.DOWNLOAD).then(function(results) {
    windowId = results.windowId;

    // Add destination directory.
    return new addEntries(['local'], [
      new TestEntryInfo(
          EntryType.DIRECTORY, null, 'destination', null, SharedOption.NONE,
          'Jan 1, 1980, 11:59 PM', 'destination', '--', 'Folder')
    ]);
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
var ITEMS_IN_DEST_DIR_AFTER_PASTE = TestEntryInfo.getExpectedRows([
  new TestEntryInfo(
      EntryType.DIRECTORY, null, 'photos',
      null, SharedOption.NONE, 'Jan 1, 1980, 11:59 PM',
      'photos', '--', 'Folder')
]);

/**
 * Navigates to specified directory on Download volume by using directory tree.
 */
function navigateWithDirectoryTree(windowId, path) {
  // Focus to directory tree.
  return remoteCall.callRemoteTestUtil(
      'focus', windowId, ['#directory-tree']).then(function() {
    // Expand download volume.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        ['[volume-type-for-testing="downloads"] .expand-icon']);
  }).then(function() {
    // Select photos directory.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        [`[full-path-for-testing="${path}"]`]);
  }).then(function() {
    // Wait until Files.app is navigated to photos.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId, `/Downloads${path}`);
  });
}

/**
 * Clicks context menu item of id in directory tree.
 */
function clickDirectoryTreeContextMenuItem(windowId, path, id) {
  // Right click photos directory.
  return remoteCall.callRemoteTestUtil('fakeMouseRightClick', windowId,
      [`[full-path-for-testing="${path}"]`]).then(function() {
    // Wait for context menu.
    return remoteCall.waitForElement(windowId,
        `#directory-tree-context-menu > [command="#${id}"]:not([disabled])`);
  }).then(function() {
    // Click menu item.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        [`#directory-tree-context-menu > [command="#${id}"]`]);
  });
}

/**
 * Navigates to destination directory and test paste operation to check whether
 * the paste operation is done correctly or not. This method does NOT check
 * source entry is deleted or not for cut operation.
 */
function navigateToDestinationDirectoryAndTestPaste(windowId) {
  // Navigates to destination directory.
  return navigateWithDirectoryTree(windowId, '/destination').then(function() {
    // Confirm files before paste.
    return remoteCall.waitForFiles(windowId, ITEMS_IN_DEST_DIR_BEFORE_PASTE,
        {ignoreLastModifiedTime: true});
  }).then(function() {
    // Paste
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'U+0056' /* v */, true /* ctrl */]);
  }).then(function() {
    // Confirm the photos directory is pasted correctly.
    return remoteCall.waitForFiles(windowId, ITEMS_IN_DEST_DIR_AFTER_PASTE,
        {ignoreLastModifiedTime: true});
  });
};

/**
 * Rename photos directory to specified name by using directory tree.
 */
function renamePhotosDirectoryTo(windowId, newName, useKeyboardShortcut) {
  return navigateWithDirectoryTree(windowId, '/photos').then(function() {
    if (useKeyboardShortcut) {
      return remoteCall.callRemoteTestUtil(
          'fakeKeyDown', windowId, ['body', 'Enter', true /* ctrl */]);
    } else {
      return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'rename');
    }
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'inputText', windowId, ['.tree-row > input', newName]);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeKeyDown', windowId, ['.tree-row > input', 'Enter', false]);
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
    return renamePhotosDirectoryTo(windowId, 'New photos', useKeyboardShortcut);
  }).then(function() {
    // Confirm that current directory has moved to new folder.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId, '/Downloads/New photos');
  });
}

/**
 * Renames directory and confirms that an alert dialog is shown.
 */
function renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(newName) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return renamePhotosDirectoryTo(windowId, newName, false);
  }).then(function() {
    // Confirm that a dialog is shown.
    return remoteCall.waitForElement(windowId, '.cr-dialog-container.shown');
  });
}

/**
 * Creates directory from directory tree.
 */
function createDirectoryFromDirectoryTree(useKeyboardShortcut) {
  var windowId;
  return setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return navigateWithDirectoryTree(windowId, '/photos');
  }).then(function() {
    if (useKeyboardShortcut) {
      return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
          ['body', 'U+0045' /* e */, true /* ctrl */]);
    } else {
      return clickDirectoryTreeContextMenuItem(
          windowId, '/photos', 'new-folder');
    }
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'inputText', windowId, ['.tree-row > input', 'test']);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeKeyDown', windowId, ['.tree-row > input', 'Enter', false]);
  }).then(function() {
    // Confirm that new directory is added to the directory tree.
    return remoteCall.waitForElement(
        windowId, '[full-path-for-testing="/photos/test"]');
  }).then(function() {
    // Confirm that current directory is not changed at this timing.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId, '/Downloads/photos');
  }).then(function() {
    // Confirm that new directory is actually created by navigating to it.
    return navigateWithDirectoryTree(windowId, '/photos/test');
  });
}

/**
 * Test case for copying a directory from directory tree by using context menu.
 */
testcase.copyFromDirectoryTreeWithContextMenu = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return navigateWithDirectoryTree(windowId, '/photos');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'copy');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Test case for copying a directory from directory tree by using keyboard
 * shortcut.
 */
testcase.copyFromDirectoryTreeWithKeyboardShortcut = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return navigateWithDirectoryTree(windowId, '/photos');
  }).then(function() {
    // Press Ctrl+C.
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'U+0043' /* c */, true /* ctrl */]);
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Test case for cutting a directory from directory tree by using context menu.
 */
testcase.cutFromDirectoryTreeWithContextMenu = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return navigateWithDirectoryTree(windowId, '/photos');
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(windowId, '/photos', 'cut');
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
    // Confirm that directory tree is updated.
    return remoteCall.waitForElementLost(
        windowId, '[full-path-for-testing="/photos"]');
  }));
};

/**
 * Test case for cutting a directory from directory tree by using keyboard
 * shortcut.
 */
testcase.cutFromDirectoryTreeWithKeyboardShortcut = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    windowId = id;
    return navigateWithDirectoryTree(windowId, '/photos');
  }).then(function() {
    // Press Ctrl+X.
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'U+0058' /* x */, true /* ctrl */]);
  }).then(function() {
    return navigateToDestinationDirectoryAndTestPaste(windowId);
  }).then(function() {
     // Confirm that directory tree is updated.
    return remoteCall.waitForElementLost(
        windowId, '[full-path-for-testing="/photos"]');
  }));
};

/**
 * Test case for pasting into folder from directory tree by using context menu.
 */
testcase.pasteIntoFolderFromDirectoryTreeWithContextMenu = function() {
  var windowId;
  testPromise(setupForDirectoryTreeContextMenuTest().then(function(id) {
    // Copy photos directory as a test data.
    windowId = id;
    return navigateWithDirectoryTree(windowId, '/photos');
  }).then(function() {
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'U+0043' /* c */, true /* ctrl */]);
  }).then(function() {
    return navigateWithDirectoryTree(windowId, '/destination');
  }).then(function() {
    // Confirm files before paste.
    return remoteCall.waitForFiles(windowId, ITEMS_IN_DEST_DIR_BEFORE_PASTE,
        {ignoreLastModifiedTime: true});
  }).then(function() {
    return clickDirectoryTreeContextMenuItem(
        windowId, '/destination', 'paste-into-folder');
  }).then(function() {
    // Confirm the photos directory is pasted correctly.
    return remoteCall.waitForFiles(windowId, ITEMS_IN_DEST_DIR_AFTER_PASTE,
        {ignoreLastModifiedTime: true});
  }).then(function() {
    // Expand the directory tree.
    return remoteCall.waitForElement(windowId,
        '[full-path-for-testing="/destination"] .expand-icon');
  }).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        ['[full-path-for-testing="/destination"] .expand-icon']);
  }).then(function() {
    // Confirm the copied directory is added to the directory tree.
    return remoteCall.waitForElement(windowId,
        '[full-path-for-testing="/destination/photos"]');
  }));
};

/**
 * Test case for renaming directory from directory tree by using context menu.
 */
testcase.renameDirectoryFromDirectoryTreeWithContextMenu = function() {
  testPromise(renameDirectoryFromDirectoryTreeSuccessCase(
      false /* do not use keyboard shortcut */));
};

/**
 * Test case for renaming directory from directory tree by using keyboard
 * shortcut.
 */
testcase.renameDirectoryFromDirectoryTreeWithKeyboardShortcut = function() {
  testPromise(renameDirectoryFromDirectoryTreeSuccessCase(
      true /* use keyboard shortcut */));
};

/**
 * Test case for renaming directory to empty string.
 */
testcase.renameDirectoryToEmptyStringFromDirectoryTree = function() {
  testPromise(renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(''));
};

/**
 * Test case for renaming directory to exsiting directory name.
 */
testcase.renameDirectoryToExistingOneFromDirectoryTree = function() {
  testPromise(renameDirectoryFromDirectoryTreeAndConfirmAlertDialog(
      'destination'));
};

/**
 * Test case for creating directory from directory tree by using context menu.
 */
testcase.createDirectoryFromDirectoryTreeWithContextMenu = function() {
  testPromise(createDirectoryFromDirectoryTree(
      false /* do not use keyboard shortcut */));
};

/**
 * Test case for creating directory from directory tree by using keyboard
 * shortcut.
 */
testcase.createDirectoryFromDirectoryTreeWithKeyboardShortcut = function() {
  testPromise(createDirectoryFromDirectoryTree(
      true /* use keyboard shortcut */));
};
