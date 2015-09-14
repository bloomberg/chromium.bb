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
