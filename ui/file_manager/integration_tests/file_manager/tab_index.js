// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests the focus behavior of the search box.
 */
testcase.searchBoxFocus = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Check that the file list has the focus on launch.
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, ['#file-list:focus']).then(this.next);
    },
    // Press the Ctrl-F key.
    function(element) {
      remoteCall.callRemoteTestUtil('fakeKeyDown',
                                    appId,
                                    ['body', 'U+0046', true],
                                    this.next);
    },
    // Check that the search box has the focus.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, ['#search-box input:focus']).
          then(this.next); },
    // Press the Esc key.
    function(element) {
      remoteCall.callRemoteTestUtil('fakeKeyDown',
                                    appId,
                                    ['#search-box input', 'U+001B', false],
                                    this.next);
    },
    // Check that the file list has the focus.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'file-list').then(this.next);
    },
    // Check for errors.
    function(result) {
      chrome.test.assertTrue(result);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests the tab focus behavior of Files.app when no file is selected.
 */
testcase.tabindexFocus = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Check that the file list has the focus on launch.
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, ['#file-list:focus']).then(this.next);
    },
    // Press the Tab key.
    function(element) {
      remoteCall.callRemoteTestUtil('getActiveElement', appId, [], this.next);
    }, function(element) {
      chrome.test.assertEq('list', element.attributes['class']);
      remoteCall.checkNextTabFocus(appId, 'search-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'view-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'gear-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'directory-tree').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'drive-welcome-link').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'file-list').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests the tab focus behavior of Files.app when no file is selected in
 * Downloads directory.
 */
testcase.tabindexFocusDownloads = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Check that the file list has the focus on launch.
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, ['#file-list:focus']).then(this.next);
    },
    // Press the Tab key.
    function(element) {
      remoteCall.callRemoteTestUtil('getActiveElement', appId, [], this.next);
    }, function(element) {
      chrome.test.assertEq('list', element.attributes['class']);
      remoteCall.checkNextTabFocus(appId, 'search-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'view-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'gear-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'directory-tree').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'file-list').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests the tab focus behavior of Files.app when a directory is selected.
 */
testcase.tabindexFocusDirectorySelected = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Check that the file list has the focus on launch.
    function(inAppId) {
      appId = inAppId;
      remoteCall.waitForElement(appId, ['#file-list:focus']).then(this.next);
    },
    // Press the Tab key.
    function(element) {
      remoteCall.callRemoteTestUtil('getActiveElement', appId, [], this.next);
    }, function(element) {
      chrome.test.assertEq('list', element.attributes['class']);
      // Select the directory named 'photos'.
      remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['photos']).then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'share-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'delete-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'search-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'view-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'gear-button').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'directory-tree').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'drive-welcome-link').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      remoteCall.checkNextTabFocus(appId, 'file-list').then(this.next);
    }, function(result) {
      chrome.test.assertTrue(result);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
