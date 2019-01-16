// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Waits that calling callRemoteTestUtil for |funcName| function with |filename|
 * returns |expectedResult|.
 * @param {string} funcName Function name for callRemoteTestUtil.
 * @param {string} filename File name to pass to callRemoteTestUtil.
 * @param {*} expectedResult Expected result for the remote call.
 * @return {Promise} Promise which will be fullfiled when the expected result is
 *     given.
 */
function waitForFunctionResult(funcName, filename, expectedResult) {
  var caller = getCaller();
  return repeatUntil(function() {
    return remoteCallVideoPlayer.callRemoteTestUtil(funcName, null, [filename])
        .then(function(result) {
          if (result === expectedResult) {
            return true;
          }
          return pending(
              caller, 'Waiting for %s return %s.', funcName, expectedResult);
        });
  });
}

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.clickControlButtons = function() {
  var openVideo = openSingleVideo('local', 'downloads', ENTRIES.world);
  var appId;
  return openVideo
      .then(function(args) {
        appId = args[0];
        // Video player starts playing given file automatically.
        return waitForFunctionResult('isPlaying', 'world.ogv', true);
      })
      .then(function() {
        // Play will finish in 2 seconds (world.ogv is 2-second short movie.)
        return waitForFunctionResult('isPlaying', 'world.ogv', false);
      })
      .then(function() {
        // Conform that clicking play button will re-play the video.
        return remoteCallVideoPlayer.callRemoteTestUtil(
            'fakeMouseClick', appId, ['.media-button.play']);
      })
      .then(function() {
        return waitForFunctionResult('isPlaying', 'world.ogv', true);
      })
      .then(function() {
        // Confirm that clicking volume button mutes the video.
        return remoteCallVideoPlayer.callRemoteTestUtil(
            'fakeMouseClick', appId, ['.media-button.sound']);
      })
      .then(function() {
        return waitForFunctionResult('isMuted', 'world.ogv', true);
      })
      .then(function() {
        // Confirm that clicking volume button again unmutes the video.
        return remoteCallVideoPlayer.callRemoteTestUtil(
            'fakeMouseClick', appId, ['.media-button.sound']);
      })
      .then(function() {
        return waitForFunctionResult('isMuted', 'world.ogv', false);
      })
      .then(function() {
        // Confirm that clicking fullscreen button enables fullscreen mode.
        return remoteCallVideoPlayer.callRemoteTestUtil(
            'fakeMouseClick', appId, ['.media-button.fullscreen']);
      })
      .then(function() {
        return remoteCallVideoPlayer.waitForElement(
            appId, '#controls[fullscreen]');
      })
      .then(function() {
        // Confirm that clicking fullscreen-exit button disables fullscreen
        // mode.
        return remoteCallVideoPlayer.callRemoteTestUtil(
            'fakeMouseClick', appId, ['.media-button.fullscreen']);
      })
      .then(function() {
        return remoteCallVideoPlayer.waitForElement(
            appId, '#controls:not([fullscreen])');
      });
};

/**
 * Confirms that native media keys are dispatched correctly.
 * @return {Promise} Promise to be fulfilled on success.
 */
testcase.mediaKeyNative = function() {
  const openVideo = openSingleVideo('local', 'downloads', ENTRIES.video);
  let appId;
  function ensurePlaying() {
    return waitForFunctionResult('isPlaying', 'video_long.ogv', true);
  }
  function ensurePaused() {
    return waitForFunctionResult('isPlaying', 'video_long.ogv', false);
  }
  function sendMediaKey() {
    return sendTestMessage({name: 'dispatchNativeMediaKey'}).then((result) => {
      chrome.test.assertEq(
          result, 'mediaKeyDispatched', 'Key dispatch failure');
    });
  }
  function pauseAndUnpause() {
    // Video player should be playing when this is called,
    return Promise.resolve()
        .then(ensurePlaying)
        .then(sendMediaKey)
        .then(ensurePaused)
        .then(sendMediaKey)
        .then(ensurePlaying);
  }
  function enableTabletMode() {
    return sendTestMessage({name: 'enableTabletMode'}).then((result) => {
      chrome.test.assertEq(result, 'tabletModeEnabled');
    });
  }
  return openVideo
      .then((args) => {
        appId = args[0];
      })
      .then(pauseAndUnpause)
      .then(enableTabletMode)
      .then(pauseAndUnpause);
};
