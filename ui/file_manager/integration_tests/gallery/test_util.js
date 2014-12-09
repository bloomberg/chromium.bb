// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Launches the Gallery app with the test entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array.<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 */
function launchWithTestEntries(
    testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    return getFilesUnderVolume(
        volumeType,
        selectedEntries.map(function(entry) { return entry.nameText; }));
  });

  return launch(entriesPromise).then(function() {
    var launchedPromise = Promise.all([appWindowPromise, entriesPromise]);
    return launchedPromise.then(function(results) {
      return {appWindow: results[0], entries: results[1]};
    });
  });
}

/**
 * Waits until the expected image is shown.
 *
 * @param {document} document Document.
 * @param {number} width Expected width of the image.
 * @param {number} height Expected height of the image.
 * @param {string} name Expected name of the image.
 * @return {Promise} Promsie to be fulfilled when the check is passed.
 */
function waitForSlideImage(document, width, height, name) {
  var expected = {width: width, height: height, name: name};
  return repeatUntil(function() {
    var fullResCanvas = document.querySelector(
        '.gallery[mode="slide"] .content canvas.fullres');
    var nameBox = document.querySelector('.namebox');
    var actual = {
      width: fullResCanvas && fullResCanvas.width,
      height: fullResCanvas && fullResCanvas.height,
      name: nameBox && nameBox.value
    };
    if (!chrome.test.checkDeepEq(expected, actual)) {
      return pending('Slide mode state, expected is %j, actual is %j.',
                     expected, actual);
    }
    return actual;
  });
}
