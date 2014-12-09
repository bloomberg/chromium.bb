// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Runs a test to open a single image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleImage(testVolumeName, volumeType) {
  var launchedPromise = launchWithTestEntries(
      testVolumeName, volumeType, [ENTRIES.desktop]);
  return launchedPromise.then(function(args) {
    var appWindow = args.appWindow;
    appWindow.resizeTo(480, 480);
    var resizedWindowPromise = repeatUntil(function() {
      if (appWindow.innerBounds.width !== 480 ||
          appWindow.innerBounds.height !== 480) {
        return pending(
            'Window bounds is expected %d x %d, but is %d x %d',
            480,
            480,
            appWindow.innerBounds.width,
            appWindow.innerBounds.height);
      }
      return appWindow;
    }).then(function(appWindow) {
      var rootElementPromise =
          waitForElement(appWindow, '.gallery[mode="slide"]');
      var imagePromise =
          waitForElement(appWindow, '.gallery .content canvas.image');
      var fullImagePromsie =
          waitForElement(appWindow, '.gallery .content canvas.fullres');
      return Promise.all([rootElementPromise, imagePromise, fullImagePromsie]).
          then(function(args) {
            chrome.test.assertEq(480, args[1].width);
            chrome.test.assertEq(360, args[1].height);
            chrome.test.assertEq(800, args[2].width);
            chrome.test.assertEq(600, args[2].height);
          });
    });
  });
}

/**
 * Runs a test to open multiple images.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openMultipleImages(testVolumeName, volumeType) {
  var testEntries = [ENTRIES.desktop, ENTRIES.image2, ENTRIES.image3];
  var launchedPromise = launchWithTestEntries(
      testVolumeName, volumeType, testEntries);
  return launchedPromise.then(function(args) {
    var appWindow = args.appWindow;
    var rootElementPromise =
        waitForElement(appWindow, '.gallery[mode="mosaic"]');
    var tilesPromise = repeatUntil(function() {
      var tiles = appWindow.contentWindow.document.querySelectorAll(
          '.mosaic-tile');
      if (tiles.length !== 3)
        return pending('The number of tiles is expected 3, but is %d',
                       tiles.length);
      return tiles;
    });
    return Promise.all([rootElementPromise, tilesPromise]);
  });
}

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleImageOnDownloads() {
  return openSingleImage('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
}

/**
 * The openSingleImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleImageOnDrive() {
  return openSingleImage('drive', VolumeManagerCommon.VolumeType.DRIVE);
}

/**
 * The openMultiImages test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openMultipleImagesOnDownloads() {
  return openMultipleImages('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
}

/**
 * The openMultiImages test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openMultipleImagesOnDrive() {
  return openMultipleImages('drive', VolumeManagerCommon.VolumeType.DRIVE);
}
