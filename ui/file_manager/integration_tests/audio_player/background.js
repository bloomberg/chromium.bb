// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of audio player.
 * @type {string}
 * @const
 */
var AUDIO_PLAYER_APP_ID = 'cjbfomnbifhcdnihkgipgfcihmgjfhbf';

var remoteCallAudioPlayer = new RemoteCall(AUDIO_PLAYER_APP_ID);

/**
 * Launches the audio player with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 * @return {Promise} Promise to be fulfilled with the audio player element.
 */
function launch(testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    var selectedEntryNames =
        selectedEntries.map(function(entry) { return entry.nameText; });
    return remoteCallAudioPlayer.getFilesUnderVolume(
        volumeType, selectedEntryNames);
  });

  var appWindow = null;
  return entriesPromise.then(function(urls) {
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'openAudioPlayer', null, [urls]);
  }).then(function(windowId) {
    appWindow = windowId;
    return remoteCallAudioPlayer.waitForElement(appWindow, 'body');
  }).then(function() {
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appWindow, 'audio-player[playing]'),
    ]);
  }).then(function(args) {
    return [appWindow, args[0]];
  });
}

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
        return testPromiseAndApps(targetTest(), [remoteCallAudioPlayer]);
      }]);
    }
  ];
  steps.shift()();
});
