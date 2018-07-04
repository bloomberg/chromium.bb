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
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
