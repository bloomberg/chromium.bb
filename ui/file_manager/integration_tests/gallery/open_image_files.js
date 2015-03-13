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
  var launchedPromise = launch(testVolumeName, volumeType, [ENTRIES.desktop]);
  return launchedPromise.then(function(args) {
    var WIDTH = 880;
    var HEIGHT = 570;
    var appId = args.appId;
    var resizedWindowPromise = gallery.callRemoteTestUtil(
        'resizeWindow', appId, [WIDTH, HEIGHT]
    ).then(function() {
      return repeatUntil(function() {
        return gallery.callRemoteTestUtil('getWindows', null, []
        ).then(function(windows) {
          var bounds = windows[appId];
          if (!bounds)
            return pending('Window is not ready yet.');

          if (bounds.outerWidth !== WIDTH || bounds.outerHeight !== HEIGHT) {
            return pending(
                'Window bounds is expected %d x %d, but is %d x %d',
                WIDTH, HEIGHT,
                bounds.outerWidth,
                bounds.outerHeight);
          }
          return true;
        });
      });
    });

    return resizedWindowPromise.then(function() {
      var rootElementPromise =
          gallery.waitForElement(appId, '.gallery[mode="slide"]');
      var imagePromise = gallery.waitForElement(
          appId, '.gallery .content canvas.image[width="760"]');
      var fullImagePromsie = gallery.waitForElement(
          appId, '.gallery .content canvas.fullres[width="800"]');
      return Promise.all([rootElementPromise, imagePromise, fullImagePromsie]).
          then(function(args) {
            chrome.test.assertEq('760', args[1].attributes.width);
            chrome.test.assertEq('570', args[1].attributes.height);
            chrome.test.assertEq('800', args[2].attributes.width);
            chrome.test.assertEq('600', args[2].attributes.height);
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
  var launchedPromise = launch(testVolumeName, volumeType, testEntries);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    var rootElementPromise =
        gallery.waitForElement(appId, '.gallery[mode="mosaic"]');
    var tilesPromise = repeatUntil(function() {
      return gallery.callRemoteTestUtil(
          'queryAllElements',
          appId,
          ['.mosaic-tile']
      ).then(function(tiles) {
        if (tiles.length !== 3)
          return pending('The number of tiles is expected 3, but is %d',
                         tiles.length);
        return tiles;
      });
    });
    return Promise.all([rootElementPromise, tilesPromise]);
  });
}

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleImageOnDownloads = function() {
  return openSingleImage('local', 'downloads');
};

/**
 * The openSingleImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleImageOnDrive = function() {
  return openSingleImage('drive', 'drive');
};

/**
 * The openMultiImages test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openMultipleImagesOnDownloads = function() {
  return openMultipleImages('local', 'downloads');
};

/**
 * The openMultiImages test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openMultipleImagesOnDrive = function() {
  return openMultipleImages('drive', 'drive');
};
