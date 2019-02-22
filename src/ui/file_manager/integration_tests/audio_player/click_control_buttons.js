// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * @param {string} query Query for an element inside <track-list> element.
 * @return {!Array<string>} Deep query selector for an element inside
 *   <track-list> polymer element.
 */
function trackListQuery(query) {
  return ['audio-player', 'track-list', query];
}

/**
 * @param {!Array<string>|string} query Query for an element inside
 *     <control-panel> element.
 * @return {!Array<string>} Deep query selector for an element inside
 *   <control-panel> polymer element.
 */
function controlPanelQuery(query) {
  return ['audio-player', 'control-panel'].concat(query);
}

/**
 * Confirms that clicking the play button changes the audio player state and
 * updates the play button label.
 * @return {Promise} Promise to be fulfilled on success.
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
        appId, [controlPanelQuery('#play[aria-label="Pause"]')]);
  }).then(function() {
    // Clicking on the play button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#play')]);
  }).then(function() {
    // ... change the audio playback state to pause,
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player:not([playing])');
  }).then(function() {
    // ... and the play button label should be 'Play'.
    return remoteCallAudioPlayer.waitForElement(
        appId, [controlPanelQuery('#play[aria-label="Play"]')]);
  }).then(function() {
    // Clicking on the play button again should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#play')]);
  }).then(function() {
    // ... change the audio playback state to playing,
    return remoteCallAudioPlayer.waitForElement(
        appId, 'audio-player[playing]');
  }).then(function() {
    // ... and the play button label should be 'Pause'.
    return remoteCallAudioPlayer.waitForElement(
        appId, [controlPanelQuery('#play[aria-label="Pause"]')]);
  });
};

/**
 * Confirms that the AudioPlayer default volume is 50, and that clicking the
 * volume button mutes / unmutes the volume.
 * @return {Promise} Promise to be fulfilled on success.
 */
testcase.changeVolumeLevel = function() {
  var openAudio = launch('local', 'downloads', [ENTRIES.beautiful]);
  var appId;
  return openAudio.then(function(args) {
    appId = args[0];
  }).then(function() {
    // The Audio Player default volume level should be 50.
    return remoteCallAudioPlayer.waitForElement(
        appId, [['audio-player', 'control-panel[volume="50"]']]);
  }).then(function() {
    // Clicking the volume button should mute the player.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#volumeButton']);
  }).then(function() {
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, [['audio-player', 'control-panel[volume="0"]']]),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#volumeButton[aria-label="Unmute"]')])
    ]);
  }).then(function() {
    // Clicking it again should unmute and restore the volume.
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, ['#volumeButton']);
  }).then(function() {
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, [['audio-player', 'control-panel[volume="50"]']]),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#volumeButton[aria-label="Mute"]')])
    ]);
  });
};

/**
 * Confirms that the Audio Player active track changes and plays when a user
 * clicks the "next" button.
 * @return {Promise} Promise to be fulfilled on success.
 */
testcase.changeTracks = function() {
  var openAudio = launch('local', 'downloads',
      [ENTRIES.beautiful, ENTRIES.newlyAdded]);
  var appId;
  return openAudio.then(function(args) {
    appId = args[0];
  }).then(function() {
    // Audio player starts playing automatically
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player[playing]'),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#play[aria-label="Pause"]')])
    ]);
  }).then(function() {
    // ... and track 0 should be active.
    return remoteCallAudioPlayer.waitForElement(
        appId, [trackListQuery('.track[index="0"][active]')]);
  }).then(function() {
    // Clicking the play button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#play')]);
  }).then(function(result) {
    chrome.test.assertTrue(!!result, 'failed to click play on track #1');
    // ... change the playback state to pause
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player:not([playing])'),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#play[aria-label="Play"]')])
    ]);
  }).then(function() {
    // ... and track 0 should still be active.
    return remoteCallAudioPlayer.waitForElement(
        appId, [trackListQuery('.track[index="0"][active]')]);
  }).then(function() {
    // Clicking the player's "next" button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#next')]);
  }).then(function(result) {
    chrome.test.assertTrue(!!result, 'failed to click play on #next button');
    // ... activate and play track 1.
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, [trackListQuery('.track[index="1"][active]')]),
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player[playing]')
    ]);
  });
};

/**
 * Confirms that the track-list expands when the play-list button is clicked,
 * then clicking on a track in the play-list, plays that track.
 * @return {Promise} Promise to be fulfilled on success.
 */
testcase.changeTracksPlayList = function() {
  var openAudio = launch('local', 'downloads',
      [ENTRIES.beautiful, ENTRIES.newlyAdded]);
  var appId;
  return openAudio.then(function(args) {
    appId = args[0];
  }).then(function() {
    // Audio player starts playing automatically
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player[playing]'),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#play[aria-label="Pause"]')])
    ]);
  }).then(function() {
    // ... and track 0 should be active.
    return remoteCallAudioPlayer.waitForElement(
        appId, [trackListQuery('.track[index="0"][active]')]);
  }).then(function() {
    // Clicking the play button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#play')]);
  }).then(function() {
    // ... change the playback state to pause,
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player:not([playing])'),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#play[aria-label="Play"]')])
    ]);
  }).then(function() {
    // ... and track 0 should still be active.
    return remoteCallAudioPlayer.waitForElement(
        appId, [trackListQuery('.track[index="0"][active]')]);
  }).then(function() {
    // Clicking the play-list button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#playList')]);
  }).then(function() {
    // ... expand the Audio Player track-list.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['audio-player', 'track-list[expanded]']);
  }).then(function() {
    // Clicking on a track (track 0 here) should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [trackListQuery('.track[index="0"]')]);
  }).then(function() {
    // ... activate and start playing that track.
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, trackListQuery('.track[index="0"][active]')),
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player[playing]')
    ]);
  });
};

/**
 * Confirms that the track-list expands when the play-list button is clicked,
 * then clicking on a track 'Play' icon in the play-list, plays that track.
 * @return {Promise} Promise to be fulfilled on success.
 */
testcase.changeTracksPlayListIcon = function() {
  var openAudio = launch('local', 'downloads',
      [ENTRIES.beautiful, ENTRIES.newlyAdded]);
  var appId;
  return openAudio.then(function(args) {
    appId = args[0];
  }).then(function() {
    // Audio player starts playing automatically
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player[playing]'),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#play[aria-label="Pause"]')])
    ]);
  }).then(function() {
    // ... and track 0 should be active.
    return remoteCallAudioPlayer.waitForElement(
        appId, [trackListQuery('.track[index="0"][active]')]);
  }).then(function() {
    // Clicking the play button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#play')]);
  }).then(function() {
    // ... change the playback state to pause,
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player:not([playing])'),
      remoteCallAudioPlayer.waitForElement(
          appId, [controlPanelQuery('#play[aria-label="Play"]')])
    ]);
  }).then(function() {
    // ... and track 0 should still be active.
    return remoteCallAudioPlayer.waitForElement(
        appId, [trackListQuery('.track[index="0"][active]')]);
  }).then(function() {
    // Clicking the play-list button should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [controlPanelQuery('#playList')]);
  }).then(function() {
    // ... expand the Audio Player track-list.
    return remoteCallAudioPlayer.waitForElement(
        appId, ['audio-player', 'track-list[expanded]']);
  }).then(function() {
    // Clicking a track (track 1 here) 'Play' icon should
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'fakeMouseClick', appId, [trackListQuery('.track[index="1"] .icon')]);
  }).then(function(result) {
    chrome.test.assertTrue(!!result, 'failed to click play on track #1');
    // ... activate and start playing that track.
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appId, trackListQuery('.track[index="1"][active]')),
      remoteCallAudioPlayer.waitForElement(
          appId, 'audio-player[playing]')
    ]);
  });
};

})();
