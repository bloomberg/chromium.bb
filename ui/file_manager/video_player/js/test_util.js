// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns if the specified file is being played.
 *
 * @param {string} filename Name of audio file to be checked. This must be same
 *     as entry.name() of the audio file.
 * @return {boolean} True if the video is playing, false otherwise.
 */
test.util.sync.isPlaying = function(filename) {
  for (var appId in window.background.appWindows) {
    var contentWindow = window.background.appWindows[appId].contentWindow;
    if (contentWindow &&
        contentWindow.document.title === filename) {
      var element = contentWindow.document.querySelector('video[src]');
      if (element && !element.paused)
        return true;
    }
  }
  return false;
};

/**
 * Loads the mock of the cast extension.
 *
 * @param {Window} contentWindow Video player window to be chacked toOB.
 */
test.util.sync.loadMockCastExtension = function(contentWindow) {
  var script = contentWindow.document.createElement('script');
  script.src =
      'chrome-extension://ljoplibgfehghmibaoaepfagnmbbfiga/' +
      'cast_extension_mock/load.js';
  contentWindow.document.body.appendChild(script);
};

/**
 * Opens the main Files.app's window and waits until it is ready.
 *
 * @param {Array.<string>} urls URLs to be opened.
 * @param {number} pos Indes in the |urls| to be played at first
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.async.openVideoPlayer = function(urls, pos, callback) {
  openVideoPlayerWindow({items: urls, position: pos}, false).then(callback);
};

/**
 * Gets file entries just under the volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<string>} names File name list.
 * @param {function(*)} callback Callback function with results returned by the
 *     script.
 */
test.util.async.getFilesUnderVolume = function(volumeType, names, callback) {
  var displayRootPromise =
      VolumeManager.getInstance().then(function(volumeManager) {
    var volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
    return volumeInfo.resolveDisplayRoot();
  });

  var retrievePromise = displayRootPromise.then(function(displayRoot) {
    var filesPromise = names.map(function(name) {
      return new Promise(
          displayRoot.getFile.bind(displayRoot, name, {}));
    });
    return Promise.all(filesPromise).then(function(aa) {
      return util.entriesToURLs(aa);
    });
  });

  retrievePromise.then(callback);
};

// Register the test utils.
test.util.registerRemoteTestUtils();
