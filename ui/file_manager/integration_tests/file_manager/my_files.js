// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests if MyFiles is displayed when flag is true.
 */
testcase.showMyFiles = function() {
  let appId;

  const expectedElementLabels = [
    'Recent: FakeItem',
    'My files: EntryListItem',
    'Downloads: SubDirectoryItem',
    'Linux files: FakeItem',
    'Google Drive: DriveVolumeItem',
    'My Drive: SubDirectoryItem',
    'Shared with me: SubDirectoryItem',
    'Offline: SubDirectoryItem',
  ];

  StepsRunner.run([
    // Open Files app on local Downloads.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.beautiful], []);
    },
    // Get the directory tree elements.
    function(results) {
      appId = results.windowId;
      const dirTreeQuery = ['#directory-tree [dir-type]'];
      remoteCall.callRemoteTestUtil('queryAllElements', appId, dirTreeQuery)
          .then(this.next);
    },
    // Check tree elements for the correct order and label/element type.
    function(elements) {
      var visibleElements = [];
      for (let element of elements) {
        if (!element.hidden) {  // Ignore hidden elements.
          visibleElements.push(
              element.attributes['entry-label'] + ': ' +
              element.attributes['dir-type']);
        }
      }
      chrome.test.assertEq(expectedElementLabels, visibleElements);
      this.next();
    },
    // Select Downloads folder.
    function() {
      remoteCall.callRemoteTestUtil(
          'selectVolume', appId, ['downloads'], this.next);
    },
    // Get the breadcrumbs elements.
    function() {
      const breadcrumbsQuery = ['#location-breadcrumbs .breadcrumb-path'];
      remoteCall.callRemoteTestUtil(
          'queryAllElements', appId, breadcrumbsQuery, this.next);
    },
    // Check that My Files is displayed on breadcrumbs.
    function(breadcrumbs) {
      const expectedBreadcrumbs = 'My files > Downloads';
      const resultBreadscrubms =
          breadcrumbs.map(crumb => crumb.text).join(' > ');
      chrome.test.assertEq(expectedBreadcrumbs, resultBreadscrubms);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests search button hidden when selected My Files.
 */
testcase.hideSearchButton = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on local Downloads.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.beautiful], []);
    },
    // Select Downloads folder.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'selectVolume', appId, ['downloads'], this.next);
    },
    // Get the search button element.
    function(result) {
      chrome.test.assertTrue(result);
      const buttonQuery = ['#search-button'];
      remoteCall.callRemoteTestUtil(
          'queryAllElements', appId, buttonQuery, this.next);
    },
    // Check that search button is visible on Downloads.
    function(buttonElements) {
      chrome.test.assertEq(1, buttonElements.length);
      chrome.test.assertFalse(buttonElements[0].hidden);
      this.next();
    },
    // Select My Files folder.
    function() {
      const myFilesQuery = '#directory-tree [entry-label="My files"]';
      const isDriveQuery = false;
      remoteCall.callRemoteTestUtil(
          'selectInDirectoryTree', appId, [myFilesQuery, isDriveQuery],
          this.next);
    },
    // Get the search button element.
    function(result) {
      chrome.test.assertTrue(result);
      const buttonQuery = ['#search-button'];
      remoteCall.callRemoteTestUtil(
          'queryAllElements', appId, buttonQuery, this.next);
    },
    // Check that search button is hidden on My Files.
    function(buttonElements) {
      chrome.test.assertEq(1, buttonElements.length);
      chrome.test.assertTrue(buttonElements[0].hidden);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests directory tree refresh doesn't hide Downloads folder.
 *
 * This tests a regression where Downloads folder would disappear because
 * MyFiles model and entry were being recreated on every update and
 * DirectoryTree expects NavigationModelItem to be the same instance through
 * updates.
 */
testcase.directoryTreeRefresh = function() {
  let appId;
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';
  StepsRunner.run([
    // Open Files app on local Downloads.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.beautiful], []);
    },
    // Mount a USB volume.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsb'}), this.next);
    },
    // Wait for the USB volume to mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Select Downloads folder.
    function() {
      remoteCall.callRemoteTestUtil(
          'selectVolume', appId, ['downloads'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests My Files displaying Downloads on file list (RHS) and opening Downloads
 * from file list.
 */
testcase.myFilesDisplaysAndOpensEntries = function() {
  let appId;
  StepsRunner.run([
    // Open Files app on local Downloads.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.beautiful], []);
    },
    // Select My Files folder.
    function(results) {
      appId = results.windowId;
      const myFilesQuery = '#directory-tree [entry-label="My files"]';
      const isDriveQuery = false;
      remoteCall.callRemoteTestUtil(
          'selectInDirectoryTree', appId, [myFilesQuery, isDriveQuery],
          this.next);
    },
    // Wait for file list to display Downloads and Crostini.
    function(result) {
      chrome.test.assertTrue(result);
      const downloadsRow = ['Downloads', '--', 'Folder'];
      const crostiniRow = ['Linux files', '--', 'Folder'];
      remoteCall
          .waitForFiles(
              appId, [downloadsRow, crostiniRow],
              {ignoreFileSize: true, ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Double click on Download on file list.
    function() {
      const downloadsFileListQuery = '#file-list [file-name="Downloads"]';
      remoteCall
          .callRemoteTestUtil(
              'fakeMouseDoubleClick', appId, [downloadsFileListQuery])
          .then(this.next);
    },
    // Wait for file list to Downloads' content.
    function() {
      remoteCall
          .waitForFiles(
              appId, [ENTRIES.beautiful.getExpectedRow()],
              {ignoreFileSize: true, ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Get the selected navigation tree item.
    function() {
      remoteCall.callRemoteTestUtil(
          'getSelectedTreeItem', appId, [], this.next);
    },
    // Get the selected navigation tree item.
    function(result) {
      chrome.test.assertEq('Downloads', result);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests My files updating its children recursively.
 *
 * If it doesn't update its children recursively it can cause directory tree to
 * not show or hide sub-folders crbug.com/864453.
 */
testcase.myFilesUpdatesChildren = function() {
  let appId;
  const downloadsQuery = '#directory-tree [entry-label="Downloads"]';
  const hiddenFolder = new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: '.hidden-folder',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: '.hidden-folder',
    sizeText: '--',
    typeText: 'Folder'
  });
  StepsRunner.run([
    // Add a hidden folder.
    function() {
      // It can't be added via setupAndWaitUntilReady, because it isn't
      // displayed and that function waits all entries to be displayed.
      addEntries(['local'], [hiddenFolder], this.next);
    },
    // Open Files app on local Downloads.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.beautiful], []);
    },
    // Select Downloads folder.
    function(results) {
      appId = results.windowId;
      const isDriveQuery = false;
      remoteCall.callRemoteTestUtil(
          'selectInDirectoryTree', appId, [downloadsQuery, isDriveQuery],
          this.next);
    },
    // Wait for gear menu to be displayed.
    function() {
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    // Open the gear menu by clicking the gear button.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    // Wait for menu to not be hidden.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for menu item to appear.
    function(result) {
      remoteCall
          .waitForElement(
              appId, '#gear-menu-toggle-hidden-files:not([disabled])')
          .then(this.next);
    },
    // Wait for menu item to appear.
    function(result) {
      remoteCall
          .waitForElement(
              appId, '#gear-menu-toggle-hidden-files:not([checked])')
          .then(this.next);
    },
    // Click the menu item.
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-toggle-hidden-files'],
          this.next);
    },
    // Check the hidden folder to be displayed in RHS.
    function(result) {
      remoteCall
          .waitForFiles(
              appId,
              TestEntryInfo.getExpectedRows([hiddenFolder, ENTRIES.beautiful]),
              {ignoreFileSize: true, ignoreLastModifiedTime: true})
          .then(this.next);
    },
    // Check the hidden folder to be displayed in LHS.
    function() {
      // Children of Downloads and named ".hidden-folder".
      const hiddenFolderTreeQuery = downloadsQuery +
          ' .tree-children .tree-item[entry-label=".hidden-folder"]';
      remoteCall.waitForElement(appId, hiddenFolderTreeQuery).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
