// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of the Files app.
 * @type {string}
 * @const
 */
var VIDEO_PLAYER_APP_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

var remoteCallVideoPlayer = new RemoteCall(VIDEO_PLAYER_APP_ID);

/**
 * Launches the video player with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array<TestEntryInfo>=} opt_selected Entries to be selected. Should
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
        'openVideoPlayer', null, [urls]);
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
 * Opens video player with a single video.
 * @param {string} volumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {TestEntryInfo} entry File to be opened.
 * @return {Promise} Promise to be fulfilled with the video player element.
 */
function openSingleVideo(volumeName, volumeType, entry) {
  var entries = [entry];
  return launch(volumeName, volumeType, entries).then(function(args) {
    var videoPlayer = args[1];

    chrome.test.assertTrue('first-video' in videoPlayer.attributes);
    chrome.test.assertTrue('last-video' in videoPlayer.attributes);
    chrome.test.assertFalse('multiple' in videoPlayer.attributes);
    chrome.test.assertFalse('disabled' in videoPlayer.attributes);
    return args;
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
        return testPromiseAndApps(targetTest(), [remoteCallVideoPlayer]);
      }]);
    }
  ];
  steps.shift()();
});
