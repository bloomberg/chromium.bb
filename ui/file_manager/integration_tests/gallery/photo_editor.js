// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Prepares the photo editor.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function setupPhotoEditor(testVolumeName, volumeType) {
  // Lauch the gallery.
  var launchedPromise = launch(
      testVolumeName,
      volumeType,
      [ENTRIES.desktop]);
  return launchedPromise.then(function(args) {
    var appId = args.appId;

    // Show the slide image.
    var slideImagePromise = gallery.waitForSlideImage(
        appId,
        800,
        600,
        'My Desktop Background');

    // Lauch the photo editor.
    var photoEditorPromise = slideImagePromise.then(function() {
      return gallery.waitAndClickElement(appId, 'button.edit');
    });

    return photoEditorPromise.then(function() {
      return args;
    });
  });
}

/**
 * Tests to rotate an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function rotateImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    return gallery.waitAndClickElement(
        appId, '.gallery:not([locked]) button.rotate_right').then(function() {
          return gallery.waitForSlideImage(
              appId,
              600,
              800,
              'My Desktop Background');
        }).
        then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) button.rotate_left');
        }).
        then(function() {
          return gallery.waitForSlideImage(
              appId,
              800,
              600,
              'My Desktop Background');
        });
  });
}

/**
 * Tests to crop an image and undoes it.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function cropImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    return gallery.waitAndClickElement(appId,
                                       '.gallery:not([locked]) button.crop').
        then(function() {
          return Promise.all([
            gallery.waitForPressEnterMessage(appId),
            gallery.waitForElement(appId, '.crop-overlay')
          ]);
        }).
        then(function() {
          return gallery.fakeKeyDown(appId, 'body', 'Enter', false);
        }).
        then(function(ret) {
          chrome.test.assertTrue(ret);
          return Promise.all([
            gallery.waitForElementLost(appId, '.prompt-wrapper .prompt'),
            gallery.waitForElementLost(appId, '.crop-overlay')
          ]);
        }).
        then(function() {
          return gallery.waitForSlideImage(
              appId,
              533,
              400,
              'My Desktop Background');
        }).
        then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) button.undo');
        }).
        then(function() {
           return gallery.waitForSlideImage(
              appId,
              800,
              600,
              'My Desktop Background');
        });
  });
}

/**
 * Tests to exposure an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function exposureImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    var url = args.urls[0];
    var buttonQuery = '.gallery:not([locked]) button.exposure';

    var origMetadata = null;

    // Click the exposure button.
    return gallery.waitAndClickElement(appId, buttonQuery).then(function() {
      // Wait until the edit controls appear.
      return Promise.all([
        gallery.waitForPressEnterMessage(appId),
        gallery.waitForElement(appId, 'input.range[name="brightness"]'),
        gallery.waitForElement(appId, 'input.range[name="contrast"]'),
      ]);
    }).then(function() {
      return gallery.callRemoteTestUtil(
          'changeValue', appId, ['input.range[name="brightness"]', 20]);
    }).then(function() {
      return gallery.callRemoteTestUtil(
          'changeValue', appId, ['input.range[name="contrast"]', -20]);
    }).then(function() {
      return gallery.callRemoteTestUtil('getMetadata', null, [url]);
    }).then(function(metadata) {
      origMetadata = metadata;

      // Push the Enter key.
      return gallery.fakeKeyDown(appId, 'body', 'Enter', false);
    }).then(function() {
      // Wait until the image is updated.
      return repeatUntil(function() {
        return gallery.callRemoteTestUtil('getMetadata', null, [url])
        .then(function(metadata) {
          if (origMetadata.modificationTime != metadata.modificationTime) {
            return true;
          } else {
            return pending(
                '%s is not updated. ' +
                    'First last modified: %s, Second last modified: %s.',
                url,
                origMetadata.modificationTime.toString(),
                metadata.modificationTime.toString());
          }
        });
      });
    });
  });
}

/**
 * The rotateImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.rotateImageOnDownloads = function() {
  return rotateImage('local', 'downloads');
};

/**
 * The rotateImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.rotateImageOnDrive = function() {
  return rotateImage('drive', 'drive');
};

/**
 * The cropImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.cropImageOnDownloads = function() {
  return cropImage('local', 'downloads');
};

/**
 * The cropImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.cropImageOnDrive = function() {
  return cropImage('drive', 'drive');
};

/**
 * The exposureImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.exposureImageOnDownloads = function() {
  return exposureImage('local', 'downloads');
};

/**
 * The exposureImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.exposureImageOnDrive = function() {
  return exposureImage('drive', 'drive');
};
