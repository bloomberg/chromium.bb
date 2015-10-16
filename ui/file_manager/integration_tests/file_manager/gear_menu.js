// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Expected files shown in Downloads with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
var BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.hiddenFile
];

/**
 * Expected files shown in Drive with hidden enabled
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
var BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument,
  ENTRIES.hiddenFile
];

/**
 * Expected files shown in Drive with Google Docs disabled
 *
 * @type {!Array<!TestEntryInfo>}
 * @const
 */
var BASIC_DRIVE_ENTRY_SET_WITHOUT_GDOCS = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
];

/**
 * Gets the common steps to toggle hidden files in the Files app
 * @param {!Array<!TestEntryInfo>} basicSet Files expected before showing hidden
 * @param {!Array<!TestEntryInfo>} hiddenEntrySet Files expected after showing
 * hidden
 * @return {!Array} The test steps to toggle hidden files
 */
function getTestCaseStepsForHiddenFiles(basicSet, hiddenEntrySet) {
  var appId;
  return [
    function(id) {
      appId = id;
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    // Click the gear menu.
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
      remoteCall.waitForElement(appId,
          '#gear-menu-toggle-hidden-files:not([disabled])').then(this.next);
    },
    // Wait for menu item to appear.
    function(result) {
      remoteCall.waitForElement(appId,
          '#gear-menu-toggle-hidden-files:not([checked])').then(this.next);
    },
    // Click the menu item.
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-toggle-hidden-files'],
              this.next);
    },
    // Wait for item to be checked.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId,
          '#gear-menu-toggle-hidden-files[checked]').then(this.next);
    },
    // Check the hidden files are displayed.
    function(result) {
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(
          hiddenEntrySet)).then(this.next);
    },
    // Repeat steps to toggle again.
    function(inAppId) {
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu:not([hidden])')
      .then(this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId,
          '#gear-menu-toggle-hidden-files:not([disabled])').then(this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId,
          '#gear-menu-toggle-hidden-files[checked]').then(this.next);
    },
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-toggle-hidden-files'],
              this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId,
          '#gear-menu-toggle-hidden-files:not([checked])').then(this.next);
    },
    function(result) {
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(basicSet))
      .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ];
}

/**
 * Test to toggle the show hidden files option on Downloads
 */
testcase.showHiddenFilesOnDownloads = function() {
  var appId;
  var steps = [
    function() {
      addEntries(['local'], BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN, this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, RootPath.DOWNLOADS).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, '#detail-table').then(this.next);
    },
    // Wait for volume list to be initialized.
    function() {
      remoteCall.waitForFileListChange(appId, 0).then(this.next);
    },
    function() {
      this.next(appId);
    },
  ];

  steps = steps.concat(getTestCaseStepsForHiddenFiles(BASIC_LOCAL_ENTRY_SET,
    BASIC_LOCAL_ENTRY_SET_WITH_HIDDEN));
  StepsRunner.run(steps);
};


/**
 * Test to toggle the show hidden files option on Drive
 */
testcase.showHiddenFilesOnDrive = function() {
  var appId;
  var steps = [
    function() {
      addEntries(['drive'], BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN, this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, RootPath.DRIVE).then(this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, '#detail-table').then(this.next);
    },
    // Wait for volume list to be initialized.
    function() {
      remoteCall.waitForFileListChange(appId, 0).then(this.next);
    },
    function() {
      this.next(appId);
    },
  ];

  steps = steps.concat(getTestCaseStepsForHiddenFiles(BASIC_DRIVE_ENTRY_SET,
    BASIC_DRIVE_ENTRY_SET_WITH_HIDDEN));
  StepsRunner.run(steps);
};

/**
 * Test to toggle the Show Google Docs option on Drive
 */
testcase.hideGoogleDocs = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE).then(this.next);
    },
    function(results) {
      appId = results.windowId;
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    // Click the gear menu.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseDown', appId, ['#gear-button'], this.next);
    },
    // Wait for menu to appear.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu').then(this.next);
    },
    // Wait for menu to appear.
    function(result) {
      remoteCall.waitForElement(appId,
          '#gear-menu-drive-hosted-settings:not([disabled])').then(this.next);
    },
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-drive-hosted-settings'],
              this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(
          BASIC_DRIVE_ENTRY_SET_WITHOUT_GDOCS)).then(this.next);
    },
    function(inAppId) {
      remoteCall.waitForElement(appId, '#gear-button').then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-button'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#gear-menu').then(this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId,
          '#gear-menu-drive-hosted-settings:not([disabled])').then(this.next);
    },
    function(results) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#gear-menu-drive-hosted-settings'],
              this.next);
    },
    function(result) {
      remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(
          BASIC_DRIVE_ENTRY_SET)).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
