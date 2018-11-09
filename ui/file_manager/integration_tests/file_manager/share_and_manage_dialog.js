// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Test 'Share with others' for a file or directory on Drive.
 *
 * @param {!string} path Path of the file or directory to be shared.
 * @param {!string} url Expected URL for the browser to visit.
 */
function shareWithOthersExpectBrowserURL(path, url) {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], BASIC_DRIVE_ENTRY_SET);
    },
    // Select the given |path|.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, [path], this.next);
    },
    // Wait for the entry to be selected.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      remoteCall.waitForElement(appId, '.table-row[selected]').then(this.next);
    },
    // Wait for the share button to appear.
    function() {
      remoteCall.waitForElement(appId, '#share-menu-button:not([disabled])')
          .then(this.next);
    },
    // Click the share button to open share menu.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#share-menu-button'], this.next);
    },
    // Check: the "Share with others" menu item should be shown enabled.
    function(result) {
      chrome.test.assertTrue(!!result);
      const shareMenuItem =
          '#share-menu:not([hidden]) [command="#share"]:not([disabled])';
      remoteCall.waitForElement(appId, shareMenuItem).then(this.next);
    },
    // Click the "Share with others" menu item.
    function(result) {
      chrome.test.assertTrue(!!result);
      const shareWithOthers = '#share-menu [command="#share"]:not([disabled])';
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [shareWithOthers], this.next);
    },
    // Wait for the share menu to disappear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      remoteCall.waitForElement(appId, '#share-menu[hidden]').then(this.next);
    },
    // Wait for the browser window to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil('getLastVisitedURL', appId, [], this.next);
    },
    // Check: the browser navigated to the expected URL.
    function(visitedUrl) {
      chrome.test.assertEq(url, visitedUrl);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Test 'Manage in Drive' for a file or directory on Drive.
 *
 * @param {!string} path Path of the file or directory to be managed.
 * @param {!string} url Expected URL for the browser to visit.
 */
function manageWithDriveExpectBrowserURL(path, url) {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], BASIC_DRIVE_ENTRY_SET);
    },
    // Select the given |path|.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('selectFile', appId, [path], this.next);
    },
    // Wait for the entry to be selected.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      remoteCall.waitForElement(appId, '.table-row[selected]').then(this.next);
    },
    // Right-click the selected entry.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]'], this.next);
    },
    // Wait for the context menu to appear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])')
          .then(this.next);
    },
    // Wait for the "Manage in Drive" menu item to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall
          .waitForElement(
              appId,
              '[command="#manage-in-drive"]:not([hidden]):not([disabled])')
          .then(this.next);
    },
    // Click the "Manage in Drive" menu item.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId,
          ['[command="#manage-in-drive"]:not([hidden]):not([disabled])'],
          this.next);
    },
    // Wait for the context menu to disappear.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      remoteCall.waitForElement(appId, '#file-context-menu[hidden]')
          .then(this.next);
    },
    // Wait for the browser window to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil('getLastVisitedURL', appId, [], this.next);
    },
    // Check: the browser navigated to the expected URL.
    function(visitedUrl) {
      chrome.test.assertEq(url, visitedUrl);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests sharing a file on Drive.
 */
testcase.shareFileDrive = function() {
  const URL = 'https://file_alternate_link/world.ogv?userstoinvite=%22%22';
  shareWithOthersExpectBrowserURL('world.ogv', URL);
};

/**
 * Tests sharing a directory on Drive.
 */
testcase.shareDirectoryDrive = function() {
  const URL = 'https://folder_alternate_link/photos?userstoinvite=%22%22';
  shareWithOthersExpectBrowserURL('photos', URL);
};

/**
 * Tests managing a file on Drive.
 */
testcase.manageFileDrive = function() {
  const URL = 'https://file_alternate_link/world.ogv';
  manageWithDriveExpectBrowserURL('world.ogv', URL);
};

/**
 * Tests managing a directory on Drive.
 */
testcase.manageDirectoryDrive = function() {
  const URL = 'https://folder_alternate_link/photos';
  manageWithDriveExpectBrowserURL('photos', URL);
};

/**
 * Tests managing a hosted file (gdoc) on Drive.
 */
testcase.manageHostedFileDrive = function() {
  const URL = 'https://document_alternate_link/Test%20Document';
  manageWithDriveExpectBrowserURL('Test Document.gdoc', URL);
};

// TODO(903637): Add tests for sharing a file on Team Drives.
