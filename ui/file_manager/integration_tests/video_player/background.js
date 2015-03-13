// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of Files.app.
 * @type {string}
 * @const
 */
var VIDEO_PLAYER_APP_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

/**
 * Extension ID of Files.app.
 * @type {string}
 * @const
 */
var FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

var remoteCallFilesApp = new RemoteCallFilesApp(FILE_MANAGER_EXTENSIONS_ID);
var remoteCallVideoPlayer = new RemoteCall(VIDEO_PLAYER_APP_ID);

/**
 * Launches the video player with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array.<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 * @return {Promise} Promise to be fulfilled with the video player element.
 */
function launch(testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    var selectedEntryNames =
        selectedEntries.map(function(entry) { return entry.nameText; });
    return remoteCallVideoPlayer.getFilesUnderVolume(
        volumeType, selectedEntryNames);
  });

  var appWindow = null;
  return entriesPromise.then(function(urls) {
    return remoteCallVideoPlayer.callRemoteTestUtil(
        'openVideoPlayer', null, [urls, 0]);
  }).then(function(windowId) {
    appWindow = windowId;
    return remoteCallVideoPlayer.waitForElement(appWindow, 'body');
  }).then(function() {
    return Promise.all([
      remoteCallVideoPlayer.waitForElement(
          appWindow, '#video-player[first-video][last-video]'),
      remoteCallVideoPlayer.waitForElement(
          appWindow, '.play.media-button[state="playing"]'),
    ]);
  }).then(function(args) {
    return [appWindow, args[0]];
  });
}

/**
 * Verifies if there are no Javascript errors in any of the app windows.
 * @param {function()} Completion callback.
 */
function checkIfNoErrorsOccured(callback) {
  var countPromise = remoteCallVideoPlayer.callRemoteTestUtil(
      'getErrorCount', null, []);
  countPromise.then(function(count) {
    chrome.test.assertEq(0, count, 'The error count is not 0.');
    callback();
  });
}

/**
 * Adds check of chrome.test to the end of the given promise.
 * @param {Promise} promise Promise.
 */
function testPromise(promise) {
  promise.then(function() {
    return new Promise(checkIfNoErrorsOccured);
  }).then(chrome.test.callbackPass(function() {
    // The callbacPass is necessary to avoid prematurely finishing tests.
    // Don't put chrome.test.succeed() here to avoid doubled success log.
  }), function(error) {
    chrome.test.fail(error.stack || error);
  });
};

/**
 * Namespace for test cases.
 */
var testcase = {};

// Ensure the test cases are loaded.
window.addEventListener('load', function() {
  var steps = [
    // Check for the guest mode.
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'isInGuestMode'}), steps.shift());
    },
    // Obtain the test case name.
    function(result) {
      if (JSON.parse(result) != chrome.extension.inIncognitoContext)
        return;
      chrome.test.sendMessage(
          JSON.stringify({name: 'getRootPaths'}), steps.shift());
    },
    // Obtain the root entry paths.
    function(result) {
      var roots = JSON.parse(result);
      RootPath.DOWNLOADS = roots.downloads;
      RootPath.DRIVE = roots.drive;
      chrome.test.sendMessage(
          JSON.stringify({name: 'getTestName'}), steps.shift());
    },
    // Run the test case.
    function(testCaseName) {
      var targetTest = testcase[testCaseName];
      if (!targetTest) {
        chrome.test.fail(testCaseName + ' is not found.');
        return;
      }
      // Specify the name of test to the test system.
      targetTest.generatedName = testCaseName;
      chrome.test.runTests([function() {
        return testPromise(targetTest());
      }]);
    }
  ];
  steps.shift()();
});
