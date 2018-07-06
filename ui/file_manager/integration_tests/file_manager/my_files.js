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
    'My Files: EntryListItem',
    'Downloads: SubDirectoryItem',
    'Linux Files: SubDirectoryItem',
    'Google Drive: DriveVolumeItem',
    'My Drive: SubDirectoryItem',
    'Shared with me: SubDirectoryItem',
    'Offline: SubDirectoryItem',
    'Add new services: MenuItem',
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
      const expectedBreadcrumbs = 'My Files > Downloads';
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
      const myFilesQuery = '#directory-tree [entry-label="My Files"]';
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
