// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Clicks the toggle-view button.
 * @param {string} windowId
 * @return {!Promise}
 */
function clickToggleViewButton(windowId) {
  return remoteCall.waitForElement(windowId, '#view-button').then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeEvent', windowId, ['#view-button', 'click']);
  });
}

/**
 * Waits until the given entry set is loaded in the current file list.
 * @param {string} windowId
 * @param {Array.<TestEntryInfo>} expectedSet
 * @param {boolean} orderCheck Whether this function waits fils appearing in the
 *     same order with expectedSet.
 */
function waitForGridViewFiles(windowId, expectedSet, orderCheck) {
  var expectedLabels = expectedSet.map(function(entryInfo) {
    return entryInfo.nameText;
  });
  return repeatUntil(function() {
    var labelsPromise = remoteCall.callRemoteTestUtil(
        'queryAllElements',
        windowId,
        ['grid:not([hidden]) .thumbnail-item .entry-name']);
    return labelsPromise.then(function(labels) {
      var actualLabels = labels.map(function(label) {
        return label.text;
      });
      if (!orderCheck) {
        expectedLabels.sort();
        actualLabels.sort();
      }
      if (chrome.test.checkDeepEq(expectedLabels, actualLabels))
        return true;
      return pending(
          'Failed to compare the grid lables, expected: %j, actual %j.',
          expectedLabels,
          actualLabels);
    });
  });
}

/**
 * Shows the grid view and checks the label texts of entries.
 *
 * @param {string} rootPath Root path to be used as a default current directory
 *     during initialization. Can be null, for no default path.
 * @param {Array.<TestEntryInfo>} expectedSet Set of entries that are expected
 *     to appear in the grid view.
 * @return {Promise} Promise to be fulfilled or rejected depending on the test
 *     result.
 */
function showGridView(rootPath, expectedSet) {
  var windowId;
  var setupPromise = setupAndWaitUntilReady(null, rootPath);
  return setupPromise.then(function(inWindowId) {
    windowId = inWindowId;
    return clickToggleViewButton(windowId);
  }).then(function() {
    // Compare the grid labels of the entries.
    return waitForGridViewFiles(windowId, expectedSet, false);
  });
}

/**
 * Checks that files are sorted by name alphabetically in grid view, and the
 * order in list view is kept on toggling the view.
 *
 * @return {Promise} Promise to be fulfilled or rejected depending on the test
 *     result.
 */
function checkFilesSortedInGridView() {
  // Entries in RootPath.DOWNLOADS, sorted by their names.
  var ENTRIES_SORTED_BY_NAME = [
    ENTRIES.photos,     // Folder: photos
    ENTRIES.beautiful,  // File: Beautiful Song.ogg (14 KB)
    ENTRIES.hello,      // File: hello.txt (51 B)
    ENTRIES.desktop,    // File: My Desktop Background.png (272 B)
    ENTRIES.world       // File: world.ogv (59 KB)
  ];
  // Entries in RootPath.DOWNLOADS, sorted by their sizes.
  var ENTRIES_SORTED_BY_SIZE = [
    ENTRIES.photos,     // Folder: photos
    ENTRIES.world,      // File: world.ogv (59 KB)
    ENTRIES.beautiful,  // File: Beautiful Song.ogg (14 KB)
    ENTRIES.desktop,    // File: My Desktop Background.png (272 B)
    ENTRIES.hello       // File: hello.txt (51 B)
  ];

  var windowId;
  return setupAndWaitUntilReady(null, RootPath.DOWNLOADS)
      .then(function(inWindowId) {
        windowId = inWindowId;
        // Sort by size by clicking column header.
        return remoteCall.callRemoteTestUtil(
            'fakeMouseClick', windowId, ['.table-header-cell:nth-of-type(2)']);
      })
      .then(function() {
        // Wait until the descending icon appears.
        return remoteCall.waitForElement(windowId,
                                         '.table-header-sort-image-desc');
      })
      .then(function() {
        // Check if the files are sorted by size.
        return remoteCall.waitForFiles(
            windowId,
            TestEntryInfo.getExpectedRows(ENTRIES_SORTED_BY_SIZE),
            {orderCheck: true});
      })
      .then(function() {
        // Change the view to the grid view
        return clickToggleViewButton(windowId);
      })
      .then(function() {
        // Check if the files are sorted by name in grid view.
        return waitForGridViewFiles(windowId, ENTRIES_SORTED_BY_NAME, true);
      })
      .then(function() {
        // Change the view to the list view.
        return clickToggleViewButton(windowId);
      })
      .then(function() {
        // Check if the sort order is restored to 'by size' in list view.
        return remoteCall.waitForFiles(
            windowId,
            TestEntryInfo.getExpectedRows(ENTRIES_SORTED_BY_SIZE),
            {orderCheck: true});
      });
}

/**
 * Tests to show grid view on a local directory.
 */
testcase.showGridViewDownloads = function() {
  testPromise(showGridView(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET));
};

/**
 * Tests to show grid view on a drive directory.
 */
testcase.showGridViewDrive = function() {
  testPromise(showGridView(
      RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET));
};

/**
 * Tests that files are always sorted by name alphabetically in grid view.
 */
testcase.checkFilesSorted = function() {
  testPromise(checkFilesSortedInGridView());
};
