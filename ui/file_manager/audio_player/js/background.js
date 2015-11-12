// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Icon of the audio player.
 * TODO(yoshiki): Consider providing an exact size icon, instead of relying
 * on downsampling by ash.
 *
 * @type {string}
 * @const
 */
var AUDIO_PLAYER_ICON = 'icons/audio-player-64.png';

var AUDIO_PLAYER_APP_URL = 'audio_player.html';

/**
 * Configuration of the audio player panel.
 * @type {Object}
 */
var audioPlayerCreateOptions = {
  id: 'audio-player',
  type: 'panel',
  minHeight: 44 + 73,  // 44px: track, 73px: controller
  minWidth: 292,
  height: 44 + 73,  // collapsed
  width: 292
};

/**
 * @constructor
 * @extends {BackgroundBase}
 */
function AudioPlayerBackground() {
  BackgroundBase.call(this);
}

AudioPlayerBackground.prototype.__proto__ = BackgroundBase.prototype;

/**
 * Called when an app is restarted.
 */
AudioPlayerBackground.prototype.onRestarted_ = function() {
  audioPlayer.reopen(function() {
    // If the audioPlayer is reopened, change its window's icon. Otherwise
    // there is no reopened window so just skip the call of setIcon.
    if (audioPlayer.rawAppWindow)
      audioPlayer.setIcon(AUDIO_PLAYER_ICON);
  });
};


/**
 * Backgound object. This is necessary for AppWindowWrapper.
 * @type {BackgroundBase}
 */
var background = new AudioPlayerBackground();

/**
 * Wrapper of audio player window.
 * @type {SingletonAppWindowWrapper}
 */
var audioPlayer = new SingletonAppWindowWrapper(AUDIO_PLAYER_APP_URL,
                                                audioPlayerCreateOptions);

/**
 * Opens player window.
 * @param {!Array<string>} urls List of audios to play and index to start
 *     playing.
 * @return {!Promise} Promise to be fulfilled on success, or rejected on error.
 */
function open(urls) {
  var position = 0;
  var startUrl = (position < urls.length) ? urls[position] : '';

  return new Promise(function(fulfill, reject) {
    if (urls.length === 0) {
      reject('No file to open.');
      return;
    }

    // Gets the current list of the children of the parent.
    window.webkitResolveLocalFileSystemURL(urls[0], function(fileEntry) {
      fileEntry.getParent(function(parentEntry) {
        var dirReader = parentEntry.createReader();
        var entries = [];

        // Call the reader.readEntries() until no more results are returned.
        var readEntries = function() {
           dirReader.readEntries(function(results) {
            if (!results.length) {
              fulfill(entries.sort(util.compareName));
            } else {
              entries = entries.concat(Array.prototype.slice.call(results, 0));
              readEntries();
            }
          }, reject);
        };

        // Start reading.
        readEntries();
      }, reject);
    }, reject);
  }).then(function(entries) {
    // Omits non-audio files.
    var audioEntries = entries.filter(FileType.isAudio);

    // Adjusts the position to start playing.
    var maybePosition = util.entriesToURLs(audioEntries).indexOf(startUrl);
    if (maybePosition !== -1)
      position = maybePosition;

    // Opens the audio player panel.
    return new Promise(function(fulfill, reject) {
      var urls = util.entriesToURLs(audioEntries);
      audioPlayer.launch({items: urls, position: position},
                         false,
                         fulfill.bind(null, null));
    });
  }).then(function() {
    audioPlayer.setIcon(AUDIO_PLAYER_ICON);
    audioPlayer.rawAppWindow.focus();
    return AUDIO_PLAYER_APP_URL;
  }).catch(function(error) {
    console.error('Launch failed' + error.stack || error);
    return Promise.reject(error);
  });
}

background.setLaunchHandler(open);
