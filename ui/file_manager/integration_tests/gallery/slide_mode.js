// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Runs a test to traverse images in the slide mode.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function traverseSlideImages(testVolumeName, volumeType) {
  var testEntries = [ENTRIES.desktop, ENTRIES.image2, ENTRIES.image3];
  var launchedPromise = launch(
      testVolumeName, volumeType, testEntries, testEntries.slice(0, 1));
  var appId;
  return launchedPromise.then(function(args) {
    appId = args.appId;
    return gallery.waitForElement(appId, '.gallery[mode="slide"]');
  }).then(function() {
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(function() {
    return gallery.waitAndClickElement(appId, '.arrow.right');
  }).then(function() {
    return gallery.waitForSlideImage(appId, 1024, 768, 'image2');
  }).then(function() {
    return gallery.waitAndClickElement(appId, '.arrow.right');
  }).then(function() {
    return gallery.waitForSlideImage(appId, 640, 480, 'image3');
  }).then(function() {
    return gallery.waitAndClickElement(appId, '.arrow.right');
  }).then(function() {
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  });
}

/**
 * Runs a test to rename an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function renameImage(testVolumeName, volumeType) {
  var launchedPromise = launch(
      testVolumeName, volumeType, [ENTRIES.desktop]);
  var appId;
  return launchedPromise.then(function(args) {
    appId = args.appId;
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(function() {
     return gallery.changeNameAndWait(appId, 'New Image Name');
  }).then(function() {
     return repeatUntil(function() {
      return gallery.getFilesUnderVolume(volumeType, ['New Image Name.png'])
      .then(function(urls) {
        if (urls.length == 1)
          return true;
        return pending('"New Image Name.png" is not found.');
      });
    });
  });
}

/**
 * Runs a test to delete an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function deleteImage(testVolumeName, volumeType) {
  var launchedPromise = launch(
      testVolumeName, volumeType, [ENTRIES.desktop]);
  var appId;
  return launchedPromise.then(function(args) {
    appId = args.appId;
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(function() {
    return gallery.waitAndClickElement(appId, 'button.delete');
  }).then(function() {
    return gallery.waitAndClickElement(appId, '.cr-dialog-ok');
  }).then(function() {
    return repeatUntil(function() {
      return gallery.getFilesUnderVolume(volumeType, ['New Image Name.png'])
      .then(function(urls) {
        if (urls.length == 0)
          return true;
        return pending('"New Image Name.png" is still there.');
      });
    });
  });
}

/**
 * The traverseSlideImages test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.traverseSlideImagesOnDownloads = function() {
  return traverseSlideImages('local', 'downloads');
};

/**
 * The traverseSlideImages test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.traverseSlideImagesOnDrive = function() {
  return traverseSlideImages('drive', 'drive');
};

/**
 * The renameImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.renameImageOnDownloads = function() {
  return renameImage('local', 'downloads');
};

/**
 * The renameImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.renameImageOnDrive = function() {
  return renameImage('drive', 'drive');
};

/**
 * The deleteImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.deleteImageOnDownloads = function() {
  return deleteImage('local', 'downloads');
};

/**
 * The deleteImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.deleteImageOnDrive = function() {
  return deleteImage('drive', 'drive');
};
