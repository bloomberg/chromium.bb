// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Test 'Share with others' for a file or directory on Drive.
 *
 * @param {!string} path Path of the file or directory to be shared.
 * @param {!string} url Expected URL for the browser to visit.
 * @param {!string|undefined} teamDrive If set, the team drive to switch to.
 */
function shareWithOthersExpectBrowserURL(path, url, teamDrive = undefined) {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [],
          BASIC_DRIVE_ENTRY_SET.concat(TEAM_DRIVE_ENTRY_SET));
    },
    // Navigate to the specified team drive if one is specified.
    function(results) {
      appId = results.windowId;
      if (!teamDrive) {
        this.next();
        return;
      }
      remoteCall
          .navigateWithDirectoryTree(
              appId, `/team_drives/${teamDrive}`, 'Team Drives', 'drive')
          .then(this.next);
    },
    // Select the given |path|.
    function() {
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
 * @param {!string|undefined} teamDrive If set, the team drive to switch to.
 */
function manageWithDriveExpectBrowserURL(path, url, teamDrive = undefined) {
  let appId;

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [],
          BASIC_DRIVE_ENTRY_SET.concat(TEAM_DRIVE_ENTRY_SET));
    },
    // Navigate to the specified team drive if one is specified.
    function(results) {
      appId = results.windowId;
      if (!teamDrive) {
        this.next();
        return;
      }
      remoteCall
          .navigateWithDirectoryTree(
              appId, `/team_drives/${teamDrive}`, 'Team Drives', 'drive')
          .then(this.next);
    },
    // Select the given |path|.
    function() {
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
 * Tests sharing a hosted file (gdoc) on Drive.
 */
testcase.shareHostedFileDrive = function() {
  const URL =
      'https://document_alternate_link/Test%20Document?userstoinvite=%22%22';
  shareWithOthersExpectBrowserURL('Test Document.gdoc', URL);
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

/**
 * Tests sharing a file in a team drive.
 */
testcase.shareFileTeamDrive = function() {
  const URL =
      'https://file_alternate_link/teamDriveAFile.txt?userstoinvite=%22%22';
  shareWithOthersExpectBrowserURL('teamDriveAFile.txt', URL, 'Team Drive A');
};

/**
 * Tests that sharing a directory in a team drive is not allowed.
 */
testcase.shareDirectoryTeamDrive = function() {
  let appId;
  const teamDrive = 'Team Drive A';
  const path = 'teamDriveADirectory';

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [],
          BASIC_DRIVE_ENTRY_SET.concat(TEAM_DRIVE_ENTRY_SET));
    },
    // Navigate to the team drive.
    function(results) {
      appId = results.windowId;
      remoteCall
          .navigateWithDirectoryTree(
              appId, `/team_drives/${teamDrive}`, 'Team Drives', 'drive')
          .then(this.next);
    },
    // Select the given |path|.
    function() {
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
    // Wait for the "Share" menu item to appear.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall
          .waitForElement(appId, '[command="#share"]:not([hidden])[disabled]')
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests sharing a hosted file (gdoc) in a team drive.
 */
testcase.shareHostedFileTeamDrive = function() {
  const URL =
      'https://document_alternate_link/teamDriveAHostedDoc?userstoinvite=%22%22';
  shareWithOthersExpectBrowserURL(
      'teamDriveAHostedDoc.gdoc', URL, 'Team Drive A');
};

/**
 * Tests managing a file in a team drive.
 */
testcase.manageFileTeamDrive = function() {
  const URL = 'https://file_alternate_link/teamDriveAFile.txt';
  manageWithDriveExpectBrowserURL('teamDriveAFile.txt', URL, 'Team Drive A');
};

/**
 * Tests managing a directory in a team drive.
 */
testcase.manageDirectoryTeamDrive = function() {
  const URL = 'https://folder_alternate_link/teamDriveADirectory';
  manageWithDriveExpectBrowserURL('teamDriveADirectory', URL, 'Team Drive A');
};

/**
 * Tests managing a hosted file (gdoc) in a team drive.
 */
testcase.manageHostedFileTeamDrive = function() {
  const URL = 'https://document_alternate_link/teamDriveAHostedDoc';
  manageWithDriveExpectBrowserURL(
      'teamDriveAHostedDoc.gdoc', URL, 'Team Drive A');
};
