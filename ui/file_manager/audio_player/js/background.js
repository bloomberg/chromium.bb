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
var AUDIO_PLAYER_ICON = 'audio_player/icons/audio-player-64.png';

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
 * Backgound object. This is necessary for AppWindowWrapper.
 * @type {BackgroundBase}
 */
var background = new BackgroundBase();

/**
 * Wrapper of audio player window.
 * @type {SingletonAppWindowWrapper}
 */
var audioPlayer = new SingletonAppWindowWrapper('audio_player.html',
                                                audioPlayerCreateOptions);

/**
 * Queue to serialize initialization.
 * @type {AsyncUtil.Queue}
 */
var initializeQueue = new AsyncUtil.Queue();

// Initializes the strings. This needs for the volume manager.
initializeQueue.run(function(fulfill) {
  chrome.fileManagerPrivate.getStrings(function(stringData) {
    loadTimeData.data = stringData;
    fulfill();
  });
});

// Initializes the volume manager. This needs for isolated entries.
initializeQueue.run(function(fulfill) {
  VolumeManager.getInstance(fulfill);
});

// Registers the handlers.
chrome.app.runtime.onLaunched.addListener(onLaunched);
chrome.app.runtime.onRestarted.addListener(onRestarted);

/**
 * Called when an app is launched.
 * @param {Object} launchData Launch data.
 */
function onLaunched(launchData) {
  if (!launchData || !launchData.items || launchData.items.length == 0)
    return;

  var playlist = {};

  initializeQueue.run(function(fulfill) {
    var isolatedEntries = launchData.items.map(function(item) {
      return item.entry;
    });

    chrome.fileManagerPrivate.resolveIsolatedEntries(isolatedEntries,
        function(externalEntries) {
          var urls = util.entriesToURLs(externalEntries);
          playlist = {items: urls, position: 0};
          fulfill();
        });
  });

  initializeQueue.run(function(fulfill) {
    open(playlist, false);
    fulfill();
  });
}

/**
 * Called when an app is restarted.
 */
function onRestarted() {
  audioPlayer.reopen(function() {
    // If the audioPlayer is reopened, change its window's icon. Otherwise
    // there is no reopened window so just skip the call of setIcon.
    if (audioPlayer.rawAppWindow)
      audioPlayer.setIcon(AUDIO_PLAYER_ICON);
  });
}

/**
 * Opens player window.
 * @param {Object} playlist List of audios to play and index to start playing.
 * @param {boolean} reopen
 * @return {Promise} Promise to be fulfilled on success, or rejected on error.
 */
function open(playlist, reopen) {
  var items = playlist.items;
  var position = playlist.position;
  var startUrl = (position < items.length) ? items[position] : '';

  return new Promise(function(fulfill, reject) {
    if (items.length === 0) {
      reject('No file to open.');
      return;
    }

    // Gets the current list of the children of the parent.
    window.webkitResolveLocalFileSystemURL(items[0], function(fileEntry) {
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
                         reopen,
                         fulfill.bind(null, null));
    });
  }).then(function() {
    audioPlayer.setIcon('icons/audio-player-64.png');
    audioPlayer.rawAppWindow.focus();
  }).catch(function(error) {
    console.error('Launch failed' + error.stack || error);
    return Promise.reject(error);
  });
}

// Register the test utils.
test.util.registerRemoteTestUtils();
