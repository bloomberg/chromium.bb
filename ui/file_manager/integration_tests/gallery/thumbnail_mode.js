// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Renames an image in thumbnail mode and confirms that thumbnail of renamed
 * image is successfully updated.
 * @param {string} testVolumeName Test volume name.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
function renameImageInThumbnailMode(testVolumeName, volumeType) {
  var launchedPromise = launch(testVolumeName, volumeType,
      [ENTRIES.desktop, ENTRIES.image3], [ENTRIES.desktop]);
  var appId;
  return launchedPromise.then(function(result) {
    // Confirm initial state after the launch.
    appId = result.appId;
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(function() {
    // Goes to thumbnail mode.
    return gallery.waitAndClickElement(appId, 'button.mode');
  }).then(function() {
    return gallery.selectImageInThumbnailMode(appId, 'image3.jpg');
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return gallery.callRemoteTestUtil('changeName', appId, ['New Image Name']);
  }).then(function() {
    // Assert that rename had done successfully.
    return gallery.waitForAFile(volumeType, 'New Image Name.jpg');
  }).then(function() {
    return gallery.selectImageInThumbnailMode(
        appId, 'My Desktop Background.png');
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(result) {
    // Only My Desktop Background.png is selected.
    chrome.test.assertEq(1, result.length);

    chrome.test.assertEq('My Desktop Background.png',
        result[0].attributes['aria-label']);
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li:not(.selected)']);
  }).then(function(result) {
    // Confirm that thumbnail of renamed image has updated.
    chrome.test.assertEq(1, result.length);
    chrome.test.assertEq('New Image Name.jpg',
        result[0].attributes['aria-label']);
  });
};

/**
 * Delete all images in thumbnail mode and confirm that no-images error banner
 * is shown.
 * @param {string} testVolumeName Test volume name.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
function deleteAllImagesInThumbnailMode(testVolumeName, volumeType) {
  var launchedPromise = launch(testVolumeName, volumeType,
      [ENTRIES.desktop, ENTRIES.image3]);
  var appId;
  return launchedPromise.then(function(result) {
    appId = result.appId;
    // Click delete button.
    return gallery.waitAndClickElement(appId, 'paper-button.delete');
  }).then(function(result) {
    chrome.test.assertTrue(!!result);
    // Wait and click delete button of confirmation dialog.
    return gallery.waitAndClickElement(appId, '.cr-dialog-ok');
  }).then(function(result) {
    chrome.test.assertTrue(!!result);
    // Wait until error banner is shown.
    return gallery.waitForElement(appId, '.gallery[error] .error-banner');
  });
}

/**
 * Rename test in thumbnail mode for Downloads.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.renameImageInThumbnailModeOnDownloads = function() {
  return renameImageInThumbnailMode('local', 'downloads');
};

/**
 * Rename test in thumbnail mode for Drive.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.renameImageInThumbnailModeOnDrive = function() {
  return renameImageInThumbnailMode('drive', 'drive');
};

/**
 * Delete all images test in thumbnail mode for Downloads.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.deleteAllImagesInThumbnailModeOnDownloads = function() {
  return deleteAllImagesInThumbnailMode('local', 'downloads');
};

/**
 * Delete all images test in thumbnail mode for Drive.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.deleteAllImagesInThumbnailModeOnDrive = function() {
  return deleteAllImagesInThumbnailMode('drive', 'drive');
};
