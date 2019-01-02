// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
testcase.deleteMenuItemNoEntrySelected = function() {
  const contextMenu = '#file-context-menu:not([hidden])';

  testPromise(setupAndWaitUntilReady(null, RootPath.DOWNLOADS).then(
      function(results) {
        var windowId = results.windowId;
        // Right click the list without selecting an entry.
        return remoteCall
            .callRemoteTestUtil('fakeMouseRightClick', windowId, ['list.list'])
            .then(function(result) {
              chrome.test.assertTrue(!!result, 'fakeMouseRightClick failed');
              // Wait until the context menu is shown.
              return remoteCall.waitForElement(windowId, contextMenu);
            })
            .then(function() {
              // Assert the menu delete command is disabled.
              const deleteDisabled = '[command="#delete"][disabled="disabled"]';
              return remoteCall.waitForElement(
                  windowId, contextMenu + ' ' + deleteDisabled);
            });
      }));
};

/**
 * Tests deleting an entry using the toolbar.
 */
testcase.deleteEntryWithToolbar = function() {
  var beforeDeletion = TestEntryInfo.getExpectedRows([
      ENTRIES.photos,
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.desktop,
      ENTRIES.beautiful
  ]);

  var afterDeletion = TestEntryInfo.getExpectedRows([
      ENTRIES.photos,
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.beautiful
  ]);

  testPromise(setupAndWaitUntilReady(null, RootPath.DOWNLOADS).then(
      function(results) {
        var windowId = results.windowId;
        // Confirm entries in the directory before the deletion.
        return remoteCall
            .waitForFiles(
                windowId, beforeDeletion, {ignoreLastModifiedTime: true})
            .then(function() {
              // Select My Desktop Background.png
              return remoteCall.callRemoteTestUtil(
                  'selectFile', windowId, ['My Desktop Background.png']);
            })
            .then(function(result) {
              chrome.test.assertTrue(result);
              // Click delete button in the toolbar.
              return remoteCall.callRemoteTestUtil(
                  'fakeMouseClick', windowId, ['button#delete-button']);
            })
            .then(function(result) {
              chrome.test.assertTrue(result);
              // Confirm that the confirmation dialog is shown.
              return remoteCall.waitForElement(
                  windowId, '.cr-dialog-container.shown');
            })
            .then(function() {
              // Press delete button.
              return remoteCall.callRemoteTestUtil(
                  'fakeMouseClick', windowId, ['button.cr-dialog-ok']);
            })
            .then(function(result) {
              chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
              // Confirm the file is removed.
              return remoteCall.waitForFiles(
                  windowId, afterDeletion, {ignoreLastModifiedTime: true});
            });
      }));
};
