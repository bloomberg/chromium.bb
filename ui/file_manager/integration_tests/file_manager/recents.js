// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

async function verifyRecents(appId, expectedRecents = RECENT_ENTRY_SET) {
  // Navigate to Recents.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['span[root-type-icon=\'recent\']']));

  // Verify Recents contains the expected files - those with an mtime in the
  // future.
  const files = TestEntryInfo.getExpectedRows(expectedRecents);
  await remoteCall.waitForFiles(appId, files);

  // Select all the files and check that the delete button isn't visible.
  // First, wait for the gear menu button to appear.
  await remoteCall.waitForElement(appId, '#gear-button');

  // Click the gear menu button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));
  // Check: #select-all command is shown, and enabled (there are files).
  await remoteCall.waitForElement(
      appId,
      '#gear-menu:not([hidden]) cr-menu-item' +
          '[command=\'#select-all\']' +
          ':not([disabled]):not([hidden])');

  // Click on the #gear-menu-select-all item.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-menu-select-all']));

  // Check: the file-list should be selected.
  await remoteCall.waitForElement(appId, '#file-list li[selected]');

  // Test that the delete button isn't visible.
  const deleteButton = await remoteCall.waitForElement(appId, '#delete-button');
  chrome.test.assertTrue(deleteButton.hidden, 'delete button should be hidden');
}

testcase.recentsDownloads = async () => {
  // Populate downloads.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  await verifyRecents(appId);
};

testcase.recentsDrive = async () => {
  // Populate drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);
  await verifyRecents(appId);
};

testcase.recentsDownloadsAndDrive = async () => {
  // Populate both downloads and drive with disjoint sets of files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful, ENTRIES.hello, ENTRIES.photos],
      [ENTRIES.desktop, ENTRIES.world, ENTRIES.testDocument]);
  await verifyRecents(appId);
};

testcase.recentsDownloadsAndDriveWithOverlap = async () => {
  // Populate both downloads and drive with overlapping sets of files.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await verifyRecents(appId, RECENT_ENTRY_SET.concat(RECENT_ENTRY_SET));
};
