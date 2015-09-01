// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Navigates to photos directory of Download volume by using directory tree.
 */
function navigateToPhotosDirectory(windowId) {
  // Focus to directory tree.
  return remoteCall.callRemoteTestUtil(
      'focus', windowId, ['#directory-tree']).then(function() {
    // Expand download volume.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        ['[volume-type-for-testing="downloads"] .expand-icon']);
  }).then(function() {
    // Select photos directory.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        ['[full-path-for-testing="/photos"]']);
  }).then(function() {
    // Wait until Files.app is navigated to photos.
    return remoteCall.waitUntilCurrentDirectoryIsChanged(
        windowId, '/Downloads/photos');
  });
};

/**
 * Navigates to downloads directory and test paste operation to check whether
 * the copy operation is done correctly or not.
 */
function navigateToDownloadsDirectoryAndTestPaste(windowId) {
  var beforeCopy = TestEntryInfo.getExpectedRows([
      ENTRIES.photos,
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.desktop,
      ENTRIES.beautiful
  ]);

  var afterCopy = TestEntryInfo.getExpectedRows([
      ENTRIES.photos,
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.desktop,
      ENTRIES.beautiful,
      new TestEntryInfo(
          EntryType.DIRECTORY, null, 'photos (1)',
          null, SharedOption.NONE, 'Jan 1, 1980, 11:59 PM',
          'photos (1)', '--', 'Folder')
  ]);

  // Click download volume.
  return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
      ['[volume-type-for-testing="downloads"]']).then(function() {
    // Confirm files before copy.
    return remoteCall.waitForFiles(windowId, beforeCopy,
        {ignoreLastModifiedTime: true});
  }).then(function() {
    // Paste
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'U+0056' /* v */, true /* ctrl */]);
  }).then(function() {
    // Confirm the photos directory is pasted correctly.
    return remoteCall.waitForFiles(windowId, afterCopy,
        {ignoreLastModifiedTime: true});
  });
};

/**
 * Test case for copying a directory from directory tree by using context menu.
 */
testcase.copyFromDirectoryTreeWithContextMenu = function() {
  var windowId;
  testPromise(setupAndWaitUntilReady(
      null, RootPath.DOWNLOAD).then(function(results) {
    windowId = results.windowId;
    return navigateToPhotosDirectory(windowId);
  }).then(function() {
    // Right click photos directory.
    return remoteCall.callRemoteTestUtil('fakeMouseRightClick', windowId,
        ['[full-path-for-testing="/photos"]']);
  }).then(function() {
    // Wait for context menu.
    return remoteCall.waitForElement(windowId,
        '#directory-tree-context-menu > [command="#copy"]:not([disabled])');
  }).then(function() {
    // Click copy item.
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId,
        ['#directory-tree-context-menu > [command="#copy"]']);
  }).then(function() {
    return navigateToDownloadsDirectoryAndTestPaste(windowId);
  }));
};

/**
 * Test case for copying a directory from directory tree by using keyboard
 * shortcut.
 */
testcase.copyFromDirectoryTreeWithKeyboardShortcut = function() {
  var windowId;
  testPromise(setupAndWaitUntilReady(
      null, RootPath.DOWNLOAD).then(function(results) {
    windowId = results.windowId;
    return navigateToPhotosDirectory(windowId);
  }).then(function() {
    // Press Ctrl+C
    return remoteCall.callRemoteTestUtil('fakeKeyDown', windowId,
        ['body', 'U+0043' /* c */, true /* ctrl */]);
  }).then(function() {
    return navigateToDownloadsDirectoryAndTestPaste(windowId);
  }));
};
