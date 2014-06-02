// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @param {Element} playerContainer Main container.
 * @param {Element} videoContainer Container for the video element.
 * @param {Element} controlsContainer Container for video controls.
 * @constructor
 */
function FullWindowVideoControls(
    playerContainer, videoContainer, controlsContainer) {
  VideoControls.call(this,
      controlsContainer,
      this.onPlaybackError_.wrap(this),
      loadTimeData.getString.wrap(loadTimeData),
      this.toggleFullScreen_.wrap(this),
      videoContainer);

  this.playerContainer_ = playerContainer;
  this.decodeErrorOccured = false;

  this.updateStyle();
  window.addEventListener('resize', this.updateStyle.wrap(this));
  document.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+0020') {  // Space
      this.togglePlayStateWithFeedback();
      e.preventDefault();
    }
    if (e.keyIdentifier == 'U+001B') {  // Escape
      util.toggleFullScreen(
          chrome.app.window.current(),
          false);  // Leave the full screen mode.
      e.preventDefault();
    }
  }.wrap(this));

  // TODO(mtomasz): Simplify. crbug.com/254318.
  videoContainer.addEventListener('click', function(e) {
    if (e.ctrlKey) {
      this.toggleLoopedModeWithFeedback(true);
      if (!this.isPlaying())
        this.togglePlayStateWithFeedback();
    } else {
      this.togglePlayStateWithFeedback();
    }
  }.wrap(this));

  this.inactivityWatcher_ = new MouseInactivityWatcher(playerContainer);
  this.__defineGetter__('inactivityWatcher', function() {
    return this.inactivityWatcher_;
  });

  this.inactivityWatcher_.check();
}

FullWindowVideoControls.prototype = { __proto__: VideoControls.prototype };

/**
 * Displays error message.
 * @param {string} message Message id.
 * @private
 */
FullWindowVideoControls.prototype.showErrorMessage_ = function(message) {
  var errorBanner = document.querySelector('#error');
  errorBanner.textContent =
      loadTimeData.getString(message);
  errorBanner.setAttribute('visible', 'true');

  // The window is hidden if the video has not loaded yet.
  chrome.app.window.current().show();
};

/**
 * Handles playback (decoder) errors.
 * @private
 */
FullWindowVideoControls.prototype.onPlaybackError_ = function() {
  this.showErrorMessage_('GALLERY_VIDEO_DECODING_ERROR');
  this.decodeErrorOccured = true;

  // Disable inactivity watcher, and disable the ui, by hiding tools manually.
  this.inactivityWatcher.disabled = true;
  document.querySelector('#video-player').setAttribute('disabled', 'true');

  // Detach the video element, since it may be unreliable and reset stored
  // current playback time.
  this.cleanup();
  this.clearState();

  // Avoid reusing a video element.
  player.unloadVideo();
};

/**
 * Toggles the full screen mode.
 * @private
 */
FullWindowVideoControls.prototype.toggleFullScreen_ = function() {
  var appWindow = chrome.app.window.current();
  util.toggleFullScreen(appWindow, !util.isFullScreen(appWindow));
};

/**
 * @constructor
 */
function VideoPlayer() {
  this.controls_ = null;
  this.videoElement_ = null;
  this.videos_ = null;

  Object.seal(this);
}

VideoPlayer.prototype = {
  get controls() {
    return this.controls_;
  }
};

/**
 * Initializes the video player window. This method must be called after DOM
 * initialization.
 * @param {Array.<Object.<string, Object>>} videos List of videos.
 */
VideoPlayer.prototype.prepare = function(videos) {
  document.ondragstart = function(e) { e.preventDefault() };

  this.videos_ = videos;

  this.controls_ = new FullWindowVideoControls(
      document.querySelector('#video-player'),
      document.querySelector('#video-container'),
      document.querySelector('#controls'));

  var reloadVideo = function(e) {
    if (this.controls_.decodeErrorOccured &&
        // Ignore shortcut keys
        !e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey) {
      this.playVideo();
      e.preventDefault();
    }
  }.wrap(this);

  document.addEventListener('keydown', reloadVideo, true);
  document.addEventListener('click', reloadVideo, true);
};

/**
 * Unloads the player.
 */
function unload() {
  if (!player.controls || !player.controls.getMedia())
    return;

  player.controls.savePosition(true /* exiting */);
  player.controls.cleanup();
}

/**
 * Loads the video file.
 * @param {string} url URL of the video file.
 * @param {string} title Title of the video file.
 * @param {function(number, number)=} opt_callback Completion callback.
 * @private
 */
VideoPlayer.prototype.loadVideo_ = function(url, title, opt_callback) {
  this.unloadVideo();

  document.title = title;

  // Re-enables ui and hide error message if already displayed.
  document.querySelector('#video-player').removeAttribute('disabled');
  document.querySelector('#error').removeAttribute('visible');
  this.controls.inactivityWatcher.disabled = false;
  this.controls.decodeErrorOccured = false;

  this.videoElement_ = document.createElement('video');
  document.querySelector('#video-container').appendChild(this.videoElement_);
  this.controls.attachMedia(this.videoElement_);

  this.videoElement_.src = url;
  this.videoElement_.load();
  if (opt_callback)
    this.videoElement_.addEventListener('loadedmetadata', opt_callback);
};

/**
 * Plays the video.
 */
VideoPlayer.prototype.playVideo = function() {
  var currentVideo = this.videos_[0];
  this.loadVideo_(currentVideo.fileUrl,
                  currentVideo.entry.name,
                  this.onVideoReady_.wrap(this));
};

/**
 * Unloads the current video.
 */
VideoPlayer.prototype.unloadVideo = function() {
  // Detach the previous video element, if exists.
  if (this.videoElement_)
    this.videoElement_.parentNode.removeChild(this.videoElement_);
  this.videoElement_ = null;
};

/**
 * Called when the video is ready after starting to load.
 * @private
 */
VideoPlayer.prototype.onVideoReady_ = function() {
  // TODO: chrome.app.window soon will be able to resize the content area.
  // Until then use approximate title bar height.
  var TITLE_HEIGHT = 33;

  var videoWidth = this.videoElement_.videoWidth;
  var videoHeight = this.videoElement_.videoHeight;

  var aspect = videoWidth / videoHeight;
  var newWidth = videoWidth;
  var newHeight = videoHeight + TITLE_HEIGHT;

  var shrinkX = newWidth / window.screen.availWidth;
  var shrinkY = newHeight / window.screen.availHeight;
  if (shrinkX > 1 || shrinkY > 1) {
    if (shrinkY > shrinkX) {
      newHeight = newHeight / shrinkY;
      newWidth = (newHeight - TITLE_HEIGHT) * aspect;
    } else {
      newWidth = newWidth / shrinkX;
      newHeight = newWidth / aspect + TITLE_HEIGHT;
    }
  }

  var oldLeft = window.screenX;
  var oldTop = window.screenY;
  var oldWidth = window.outerWidth;
  var oldHeight = window.outerHeight;

  if (!oldWidth && !oldHeight) {
    oldLeft = window.screen.availWidth / 2;
    oldTop = window.screen.availHeight / 2;
  }

  var appWindow = chrome.app.window.current();
  appWindow.resizeTo(newWidth, newHeight);
  appWindow.moveTo(oldLeft - (newWidth - oldWidth) / 2,
                   oldTop - (newHeight - oldHeight) / 2);
  appWindow.show();

  this.videoElement_.play();
};

/**
 * Initialize the list of videos.
 * @param {function(Array.<Object>)} callback Called with the video list when
 *     it is ready.
 **/
function initVideos(callback) {
  if (window.videos) {
    var videos = window.videos;
    window.videos = null;
    callback(videos);
    return;
  }

  chrome.runtime.onMessage.addListener(
      function(request, sender, sendResponse) {
        var videos = window.videos;
        window.videos = null;
        callback(videos);
      });
}

var player = new VideoPlayer();

/**
 * Initializes the strings.
 * @param {function()} callback Called when the sting data is ready.
 **/
function initStrings(callback) {
  chrome.fileBrowserPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;
    callback();
  });
}

var initPromise = Promise.all(
    [new Promise(initVideos.wrap(null)),
     new Promise(initStrings.wrap(null)),
     new Promise(util.addPageLoadHandler.wrap(null))]);

initPromise.then(function(results) {
  var videos = results[0];
  player.prepare(videos);
  return new Promise(player.playVideo.wrap(player));
});
