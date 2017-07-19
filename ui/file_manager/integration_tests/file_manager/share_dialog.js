// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Test sharing dialog for a file or directory on Drive
 * @param {string} path Path for a file or a directory to be shared.
 */
function share(path) {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Select the source file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'selectFile', appId, [path], this.next);
    },
    // Wait for the share button.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#share-menu-button:not([disabled])')
          .then(this.next);
    },
    // Open share options menu
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#share-menu-button'], this.next);
    },
    // Wait until the "Share with others" item is shown.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall
          .waitForElement(
              appId, 'cr-menu-item[command="#share"]:not([disabled]')
          .then(this.next);
    },
    // Invoke the share dialog.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['cr-menu-item[command="#share"]'],
          this.next);
    },
    // Wait until the share dialog's contents are shown.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '.share-dialog-webview-wrapper.loaded').
          then(this.next);
    },
    function(result) {
      chrome.test.assertTrue(!!result);
      repeatUntil(function() {
        return remoteCall.callRemoteTestUtil(
            'queryAllElements',
            appId,
            [
              '.share-dialog-webview-wrapper.loaded',
              null /* iframe */,
              ['width', 'height']
            ]).
            then(function(elements) {
          // TODO(mtomasz): Fix the wrong geometry of the share dialog.
          // return elements[0] &&
          //     elements[0].styles.width === '350px' &&
          //     elements[0].styles.height === '250px' ?
          //     undefined :
          //     pending('Dialog wrapper is currently %j. ' +
          //             'but should be: 350x250',
          //             elements[0]);
          return elements[0] ?
              undefined : pending('The share dialog is not found.');
        });
      }).
      then(this.next);
    },
    // Wait until the share dialog's contents are shown.
    function(result) {
      remoteCall.callRemoteTestUtil(
          'executeScriptInWebView',
          appId,
          ['.share-dialog-webview-wrapper.loaded webview',
           'document.querySelector("button").click()'],
          this.next);
    },
    // Wait until the share dialog's contents are hidden.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElementLost(
          appId, '.share-dialog-webview-wrapper.loaded').
          then(this.next);
    },
    // Check for Javascript errros.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests sharing a file on Drive
 */
testcase.shareFile = share.bind(null, 'world.ogv');

/**
 * Tests sharing a directory on Drive
 */
testcase.shareDirectory = share.bind(null, 'photos');
