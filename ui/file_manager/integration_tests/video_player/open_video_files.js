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
    chrome.test.assertTrue(videoPlayer.hasAttribute('first-video'));
    chrome.test.assertTrue(videoPlayer.hasAttribute('last-video'));
    chrome.test.assertFalse(videoPlayer.hasAttribute('multiple'));
    chrome.test.assertFalse(videoPlayer.hasAttribute('disabled'));
    return args;
  });
}

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleVideoOnDownloads() {
  var test = openSingleVideo('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
  return test.then(function(args) {
    var videoPlayer = args[1];
    chrome.test.assertFalse(videoPlayer.hasAttribute('cast-available'));
  });
}

/**
 * The openSingleImage test for Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleVideoOnDrive() {
  var test = openSingleVideo('drive', VolumeManagerCommon.VolumeType.DRIVE);
  return test.then(function(args) {
    var appWindow = args[0];
    var videoPlayer = args[1];
    chrome.test.assertFalse(videoPlayer.hasAttribute('cast-available'));

    // Loads cast extension and wait for available cast.
    loadMockCastExtesntion(appWindow);
    return waitForElement(appWindow, '#video-player[cast-available]');
  });
}
