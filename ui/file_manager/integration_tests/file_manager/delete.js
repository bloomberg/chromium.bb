// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
testcase.deleteMenuItemNoEntrySelected = function() {
  testPromise(setupAndWaitUntilReady(null, RootPath.DOWNLOADS).then(
      function(results) {
        var windowId = results.windowId;
        // Right click the list without selecting an item.
        return remoteCall.callRemoteTestUtil(
            'fakeMouseRightClick', windowId, ['list.list']
            ).then(function(result) {
          chrome.test.assertTrue(result);

          // Wait until the context menu is shown.
          return remoteCall.waitForElement(
              windowId,
              '#file-context-menu:not([hidden])');
        }).then(function() {
          // Assert that delete command is disabled.
          return remoteCall.waitForElement(
              windowId,
              'cr-menu-item[command="#delete"][disabled="disabled"]');
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
        //
        // Ignore last modified time since file manager sometimes fails to get
        // last modified time of files.
        // TODO(yawano): Fix the root cause and remove this temporary fix.
        return remoteCall.waitForFiles(windowId, beforeDeletion,
            {ignoreLastModifiedTime: true}).then(function() {
          // Select My Desktop Background.png
          return remoteCall.callRemoteTestUtil(
              'selectFile', windowId, ['My Desktop Background.png']);
        }).then(function(result) {
          chrome.test.assertTrue(result);

          // Click delete button in the toolbar.
          return remoteCall.callRemoteTestUtil(
              'fakeMouseClick', windowId, ['button#delete-button']);
        }).then(function(result) {
          chrome.test.assertTrue(result);

          // Confirm that the confirmation dialog is shown.
          return remoteCall.waitForElement(
              windowId, '.cr-dialog-container.shown');
        }).then(function() {
          // Press delete button.
          return remoteCall.callRemoteTestUtil(
              'fakeMouseClick', windowId, ['button.cr-dialog-ok']);
        }).then(function() {
          // Confirm the file is removed.
          return remoteCall.waitForFiles(windowId, afterDeletion,
              {ignoreLastModifiedTime: true});
        });
      }));
};

/**
 * Tests that the Delete menu item is in |deleteEnabledState| when the entry
 * at |path| is selected.
 *
 * @param {string} path Path to the file to try and delete.
 * @param {boolean} deleteEnabledState True if the delete command should be
 *     enabled in the context menu, false if not.
 */
function deleteContextMenu(path, deleteEnabledState) {
  var appId;
  StepsRunner.run([
    // Set up Files App.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, BASIC_LOCAL_ENTRY_SET,
          COMPLEX_DRIVE_ENTRY_SET);
    },
    // Select the read-only file.
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
    // Wait for the "Delete" option to appear.
    function() {
      var query;
      if (deleteEnabledState) {
        query = '[command="#delete"]:not([hidden]):not([disabled])';
      } else {
        query = '[command="#delete"][disabled]:not([hidden])';
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
 * Tests that the Delete menu item is disabled if a read-only document is
 * selected.
 */
testcase.deleteContextMenuItemDisabledForReadOnlyDocument = function() {
  deleteContextMenu('Read-Only Doc.gdoc', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only file is selected.
 */
testcase.deleteContextMenuItemDisabledForReadOnlyFile = function() {
  deleteContextMenu('Read-Only File.jpg', false);
};

/**
 * Tests that the Delete menu item is disabled if a read-only folder is
 * selected.
 */
testcase.deleteContextMenuItemDisabledForReadOnlyFolder = function() {
  deleteContextMenu('Read-Only Folder', false);
};

/**
 * Tests that the Delete menu item is enabled if a read-write entry is selected.
 */
testcase.deleteContextMenuItemEnabledForReadWriteFile = function() {
  deleteContextMenu('hello.txt', true);
};
