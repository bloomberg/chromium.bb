// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Stores the app windows OLNY for test purpose.
// We SHOULD NOT use it as it is except for test, since the files which have
// the same name will be overridden each other.
var appWindowsForTest = {};

var initializeQueue = new AsyncUtil.Queue();

// Initializes the strings. This needs for the volume manager.
initializeQueue.run(function(fulfill) {
  chrome.fileBrowserPrivate.getStrings(function(stringData) {
    loadTimeData.data = stringData;
    fulfill();
  }.wrap());
}.wrap());

// Initializes the volume manager. This needs for isolated entries.
initializeQueue.run(function(fulfill) {
  VolumeManager.getInstance(fulfill);
}.wrap());

chrome.app.runtime.onLaunched.addListener(onLaunched);

/**
 * Called when an app is launched.
 * @param {Object} launchData Launch data.
 */
function onLaunched(launchData) {
  if (!launchData || !launchData.items || launchData.items.length == 0)
    return;

  var videos = [];

  initializeQueue.run(function(fulfill) {
    var isolatedEntries = launchData.items.map(function(item) {
      return item.entry;
    });

    chrome.fileBrowserPrivate.resolveIsolatedEntries(isolatedEntries,
        function(externalEntries) {
          videos = externalEntries.map(function(entry) {
            return Object.freeze({
              entry: entry,
              title: entry.name,
              url: entry.toURL(),
            });
          });
          fulfill();
        }.wrap());
  }.wrap());

  initializeQueue.run(function(fulfill) {
    if (videos.length > 0) {
      open(videos);
    } else {
      // TODO(yoshiki): handle error in a smarter way.
      open('', 'error');  // Empty URL shows the error message.
    }
    fulfill();
  }.wrap());
}

/**
 * Opens player window.
 * @param {Array.<Object>} videos List of videos to play.
 * @param {Promise} Promise to be fulfilled on success, or rejected on error.
 */
function open(videos) {
  return new Promise(function(fulfill, reject) {
    chrome.app.window.create('video_player.html', {
      id: 'video',
      frame: 'none',
      singleton: false,
      minWidth: 480,
      minHeight: 270
    },
    fulfill);
  }).then(function(createdWindow) {
    // Stores the window for test purpose.
    appWindowsForTest[videos[0].entry.name] = createdWindow;

    createdWindow.setIcon('images/icon/video-player-64.png');
    createdWindow.contentWindow.videos = videos;
    chrome.runtime.sendMessage({ready: true}, function() {});
  }).catch(function(error) {
    console.error('Launch failed', error.stack || error);
    return Promise.reject(error);
  });
}

// If is is run in the browser test, wait for the test resources are installed
// as a component extension, and then load the test resources.
if (chrome.test) {
  window.testExtensionId = 'ljoplibgfehghmibaoaepfagnmbbfiga';
  chrome.runtime.onMessageExternal.addListener(function(message) {
    if (message.name !== 'testResourceLoaded')
      return;
    var script = document.createElement('script');
    script.src =
        'chrome-extension://' + window.testExtensionId +
        '/common/test_loader.js';
    document.documentElement.appendChild(script);
  });
}
