// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Icon of the video player.
 * TODO(yoshiki): Consider providing an exact size icon, instead of relying
 * on downsampling by ash.
 *
 * @type {string}
 * @const
 */
var ICON_IMAGE = 'images/icon/video-player-64.png';

/**
 * Configuration of the video player panel.
 * @type {Object}
 */
var windowCreateOptions = {
  frame: 'none',
  minWidth: 480,
  minHeight: 270
};

/**
 * Backgound object. This is necessary for AppWindowWrapper.
 * @type {BackgroundBase}
 */
var background = new BackgroundBase();

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
  }.wrap());
}.wrap());

// Initializes the volume manager. This needs for isolated entries.
initializeQueue.run(function(fulfill) {
  VolumeManager.getInstance(fulfill);
}.wrap());

// Registers the handlers.
chrome.app.runtime.onLaunched.addListener(onLaunched);

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
    }.wrap());

    chrome.fileManagerPrivate.resolveIsolatedEntries(isolatedEntries,
        function(externalEntries) {
          var urls = util.entriesToURLs(externalEntries);
          playlist = {items: urls, position: 0};
          fulfill();
        }.wrap());
  }.wrap());

  initializeQueue.run(function(fulfill) {
    openVideoPlayerWindow(playlist, false);
    fulfill();
  }.wrap());
}

var generateWindowId = (function() {
  var seq = 0;
  return function() {
    return 'VIDEO_PLAYER_APP_' + seq++;
  }.wrap();
}.wrap())();

/**
 * Opens player window.
 * @param {Object} playlist List of videos to play and index to start playing.
 * @param {boolean} reopen True if reopen, false otherwise.
 * @return {Promise} Promise to be fulfilled on success, or rejected on error.
 */
function openVideoPlayerWindow(playlist, reopen) {
  var items = playlist.items;
  var position = playlist.position;
  var startUrl = (position < items.length) ? items[position] : '';
  var windowId = null;

  return new Promise(function(fulfill, reject) {
    util.URLsToEntries(items).then(function(result) {
      fulfill(result.entries);
    }.wrap()).catch(reject);
  }.wrap()).then(function(entries) {
    if (entries.length === 0)
      return Promise.reject('No file to open.');

    // Adjusts the position to start playing.
    var maybePosition = util.entriesToURLs(entries).indexOf(startUrl);
    if (maybePosition !== -1)
      position = maybePosition;

    windowId = generateWindowId();

    // Opens the video player window.
    return new Promise(function(fulfill, reject) {
      var urls = util.entriesToURLs(entries);
      var videoPlayer = new AppWindowWrapper('video_player.html',
          windowId,
          windowCreateOptions);

      videoPlayer.launch(
          {items: urls, position: position},
          reopen,
          fulfill.bind(null, videoPlayer));
    }.wrap());
  }.wrap()).then(function(videoPlayer) {
    var appWindow = videoPlayer.rawAppWindow;

    if (chrome.test)
      appWindow.contentWindow.loadMockCastExtensionForTest = true;

    videoPlayer.setIcon(ICON_IMAGE);
    appWindow.focus();

    return windowId;
  }.wrap()).catch(function(error) {
    console.error('Launch failed' + error.stack || error);
    return Promise.reject(error);
  }.wrap());
}
