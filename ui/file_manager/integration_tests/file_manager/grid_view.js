// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Shows the grid view and checks the label texts of entries.
 *
 * @param {string} rootPath Root path to be used as a default current directory
 *     during initialization. Can be null, for no default path.
 * @param {Array<TestEntryInfo>} expectedSet Set of entries that are expected
 *     to appear in the grid view.
 * @return {Promise} Promise to be fulfilled or rejected depending on the test
 *     result.
 */
async function showGridView(rootPath, expectedSet) {
  const caller = getCaller();
  const expectedLabels =
      expectedSet.map((entryInfo) => entryInfo.nameText).sort();

  // Open Files app on |rootPath|.
  const appId = await setupAndWaitUntilReady(rootPath);

  // Click the grid view button.
  await remoteCall.waitForElement(appId, '#view-button');
  await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#view-button', 'click']);

  // Compare the grid labels of the entries.
  await repeatUntil(async () => {
    const labels = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId,
        ['grid:not([hidden]) .thumbnail-item .entry-name']);
    const actualLabels = labels.map((label) => label.text).sort();

    if (chrome.test.checkDeepEq(expectedLabels, actualLabels)) {
      return true;
    }
    return pending(
        caller, 'Failed to compare the grid lables, expected: %j, actual %j.',
        expectedLabels, actualLabels);
  });
}

/**
 * Tests to show grid view on a local directory.
 */
testcase.showGridViewDownloads = function() {
  return showGridView(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests to show grid view on a drive directory.
 */
testcase.showGridViewDrive = function() {
  return showGridView(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET);
};
