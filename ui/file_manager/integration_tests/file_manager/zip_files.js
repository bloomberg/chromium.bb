// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Returns the file ENTRIES.zipArchive content (2 files) as row entries.
 */
function getZipArchiveFileRowEntries() {
  return [
    ['image.png', '272 bytes', 'PNG image', 'Sep 2, 2013, 10:01 PM'],
    ['text.txt', '51 bytes', 'Plain text', 'Sep 2, 2013, 10:01 PM']
  ];
}

/**
 * Tests zip file open (aka unzip) from Downloads.
 */
testcase.zipFileOpenDownloads = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Downloads containing a zip file.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.zipArchive], []);
    },
    // Select the zip file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['archive.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getZipArchiveFileRowEntries();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};

/**
 * Tests zip file open (aka unzip) from Google Drive.
 */
testcase.zipFileOpenDrive = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive containing a zip file.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.zipArchive]);
    },
    // Select the zip file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, ['archive.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getZipArchiveFileRowEntries();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};

/**
 * Tests zip file open (aka unzip) from a removable USB volume.
 */
testcase.zipFileOpenUsb = function() {
  let appId;

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], [ENTRIES.beautiful]);
    },
    // Mount empty USB volume in the Drive window.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsbEmpty'}), this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY], this.next);
    },
    // Add zip file to the USB volume.
    function() {
      addEntries(['usb'], [ENTRIES.zipArchive], this.next);
    },
    // Verify the USB file list.
    function() {
      const archive = [ENTRIES.zipArchive.getExpectedRow()];
      remoteCall.waitForFiles(appId, archive).then(this.next);
    },
    // Select the zip file.
    function() {
      remoteCall.callRemoteTestUtil('selectFile', appId, ['archive.zip'])
          .then(this.next);
    },
    // Press the Enter key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const key = ['#file-list', 'Enter', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key, this.next);
    },
    // Check: the zip file content should be shown (unzip).
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      const files = getZipArchiveFileRowEntries();
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
  ]);
};
