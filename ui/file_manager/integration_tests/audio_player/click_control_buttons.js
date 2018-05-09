// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Confirms that clicking the play button changes the audio player state and
 * updates the play button's label.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.togglePlayState = function() {
  var openAudio = launch('local', 'downloads', [ENTRIES.beautiful]);
  var appId;
  return openAudio.then(function(args) {
    appId = args[0];
  }).then(function() {
    // Audio player should start playing automatically,
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player[playing]');
  }).then(function() {
    // .. and the play button label should be 'Pause'.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['#play[aria-label="Pause"]']);
  }).then(function() {
    // Clicking on the play button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#play']);
  }).then(function() {
    // ... change the audio playback state to pause,
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player:not([playing])');
  }).then(function() {
    // ... and the play button label should be 'Play'.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['#play[aria-label="Play"]']);
  }).then(function() {
    // Clicking on the play button again should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#play']);
  }).then(function() {
    // ... change the audio playback state to playing,
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player[playing]');
  }).then(function() {
    // ... and the play button label should be 'Pause'.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['#play[aria-label="Pause"]']);
  });
};

/**
 * Confirms that the AudioPlayer default volume is 50 and that clicking the
 * volume button mutes / unmutes the volume label.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.changeVolumeLevel = function() {
  var openAudio = launch('local', 'downloads', [ENTRIES.beautiful]);
  var appId;
  return openAudio.then(function(args) {
    appId = args[0];
  }).then(function() {
    // The Audio Player default volume level should be 50.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['control-panel[volume="50"]']);
  }).then(function() {
    // Clicking the volume button should mute the player.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#volumeButton']);
  }).then(function() {
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, ['control-panel[volume="0"]']),
      remoteCallAudioPlayer.waitForElement(
          appId, ['#volumeButton[aria-label="Unmute"]'])
    ]);
  }).then(function() {
    // Clicking it again should unmute and restore the volume.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#volumeButton']);
  }).then(function() {
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, ['control-panel[volume="50"]']),
      remoteCallAudioPlayer.waitForElement(
          appId, ['#volumeButton[aria-label="Mute"]'])
    ]);
  });
};

/**
 * Confirm that clicking "Next" and track on playlist change the current track
 * and play state correctly.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.changeTracks = function() {
  var openAudio = launch('local', 'downloads',
      [ENTRIES.beautiful, ENTRIES.newlyAdded]);
  var appId;
  return openAudio.then(function(args) {
    appId = args[0];
  }).then(function() {
    // While playing, the play/pause button should have 'Pause' label.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['#play[aria-label="Pause"]']);
  }).then(function() {
    // Clicking the pause button should change the playback state to pause.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#play']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player:not([playing])');
  }).then(function() {
    // The first track should be active.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['.track[index="0"][active]']);
  }).then(function() {
    // Clicking next button should activate the second track and start playing.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#next']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, ['.track[index="1"][active]']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player[playing]');
  }).then(function() {
    // Pause to prepare for remaining steps.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#play']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player:not([playing])');
  }).then(function() {
    // Clicking playlist button should expand track list.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#playList']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, 'track-list[expanded]');
  }).then(function() {
    // Clicking the first track should make it active and start playing it.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['.track[index="0"]']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, '.track[index="0"][active]');
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player[playing]');
  }).then(function() {
    // Pause to prepare for remaining steps.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#play']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player:not([playing])');
  }).then(function() {
    // Clicking the play icon on the second should make it active, and start
    // playing.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['.track[index="1"] .icon']);
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, '.track[index="1"][active]');
  }).then(function() {
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player[playing]');
  });
};
