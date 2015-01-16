// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Opens video player with a single video.
 * @param {string} volumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with the video player element.
 */
function openSingleVideo(volumeName, volumeType) {
  var entries = [ENTRIES.world];
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
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleVideoOnDownloads = function() {
    var test = openSingleVideo('local', 'downloads');
    return test.then(function(args) {
      var videoPlayer = args[1];
      chrome.test.assertFalse('cast-available' in videoPlayer.attributes);
    });
};

/**
 * The openSingleImage test for Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleVideoOnDrive = function() {
    var test = openSingleVideo('drive', 'drive');
    return test.then(function(args) {
      var appWindow = args[0];
      var videoPlayer = args[1];
      chrome.test.assertFalse('cast-available' in videoPlayer.attributes);

      return remoteCallVideoPlayer.callRemoteTestUtil(
          'loadMockCastExtension', appWindow, []).then(function() {
        // Loads cast extension and wait for available cast.
        return remoteCallVideoPlayer.waitForElement(
            appWindow, '#video-player[cast-available]');
      });
    });
};
