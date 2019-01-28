// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Video player with native controls
 *
 * @constructor
 * @struct
 */
function NativeControlsVideoPlayer() {
  this.videos_ = null;
  this.currentPos_ = 0;
  this.videoElement_ = null;
}

/**
 * Initializes the video player window. This method must be called after DOM
 * initialization.
 * @param {!Array<!FileEntry>} videos List of videos.
 */
NativeControlsVideoPlayer.prototype.prepare = function(videos) {
  this.videos_ = videos;

  // TODO: Move these setting to html and css file when
  // we are confident to remove the feature flag
  this.videoElement_ = document.createElement('video');
  this.videoElement_.controls = true;
  this.videoElement_.controlsList = 'nodownload';
  this.videoElement_.style.pointerEvents = 'auto';
  getRequiredElement('video-container').appendChild(this.videoElement_);

  // TODO: remove the element in html when remove the feature flag
  getRequiredElement('controls-wrapper').style.display = 'none';
  getRequiredElement('spinner-container').style.display = 'none';
  getRequiredElement('error-wrapper').style.display = 'none';
  getRequiredElement('thumbnail').style.display = 'none';
  getRequiredElement('cast-container').style.display = 'none';

  this.videoElement_.addEventListener('pause', this.onPause_.bind(this));

  this.preparePlayList_();
  this.addKeyControls_();
};

/**
 * 10 seconds should be skipped when J/L key is pressed.
 */
NativeControlsVideoPlayer.PROGRESS_MAX_SECONDS_TO_SKIP = 10;

/**
 * 20% of duration should be skipped when the video is too short to skip 10
 * seconds.
 */
NativeControlsVideoPlayer.PROGRESS_MAX_RATIO_TO_SKIP = 0.2;

/**
 * Attach arrow box for previous/next track to document and set
 * 'multiple' attribute if user opens more than 1 videos.
 */
NativeControlsVideoPlayer.prototype.preparePlayList_ = function() {
  let videoPlayerElement = getRequiredElement('video-player');
  if (this.videos_.length > 1) {
    videoPlayerElement.setAttribute('multiple', true);
  } else {
    videoPlayerElement.removeAttribute('multiple');
  }

  let arrowRight = queryRequiredElement('.arrow-box .arrow.right');
  arrowRight.addEventListener(
      'click', this.advance_.wrap(this, true /* next track */));
  let arrowLeft = queryRequiredElement('.arrow-box .arrow.left');
  arrowLeft.addEventListener(
      'click', this.advance_.wrap(this, false /* previous track */));
};

/**
 * Add keyboard controls to document.
 */
NativeControlsVideoPlayer.prototype.addKeyControls_ = function() {
  document.addEventListener('keydown', function(e) {
    switch (util.getKeyModifiers(e) + e.key) {
      // Handle debug shortcut keys.
      case 'Ctrl-Shift-I':  // Ctrl+Shift+I
        chrome.fileManagerPrivate.openInspector('normal');
        break;
      case 'Ctrl-Shift-J':  // Ctrl+Shift+J
        chrome.fileManagerPrivate.openInspector('console');
        break;
      case 'Ctrl-Shift-C':  // Ctrl+Shift+C
        chrome.fileManagerPrivate.openInspector('element');
        break;
      case 'Ctrl-Shift-B':  // Ctrl+Shift+B
        chrome.fileManagerPrivate.openInspector('background');
        break;

      case 'k':
      case 'MediaPlayPause':
        this.togglePlayState_();
        break;
      case 'Escape':
        util.toggleFullScreen(
            chrome.app.window.current(),
            false);  // Leave the full screen mode.
        break;
      case 'MediaTrackNext':
        this.advance_(true /* next track */);
        break;
      case 'MediaTrackPrevious':
        this.advance_(false /* previous track */);
        break;
      case 'l':
        this.skip_(true /* forward */);
        break;
      case 'j':
        this.skip_(false /* backward */);
        break;
      case 'BrowserBack':
        chrome.app.window.current().close();
        break;
      case 'MediaStop':
        // TODO: Define "Stop" behavior.
        break;
    }
  }.wrap(this));
};

/**
 * Skips forward/backward.
 * @param {boolean} forward Whether to skip forward or backward.
 * @private
 */
NativeControlsVideoPlayer.prototype.skip_ = function(forward) {
  let secondsToSkip = Math.min(
      NativeControlsVideoPlayer.PROGRESS_MAX_SECONDS_TO_SKIP,
      this.videoElement_.duration *
          NativeControlsVideoPlayer.PROGRESS_MAX_RATIO_TO_SKIP);

  if (!forward) {
    secondsToSkip *= -1;
  }

  this.videoElement_.currentTime = Math.max(
      Math.min(
          this.videoElement_.currentTime + secondsToSkip,
          this.videoElement_.duration),
      0);
};

/**
 * Toggle play/pause.
 */
NativeControlsVideoPlayer.prototype.togglePlayState_ = function() {
  if (this.videoElement_.paused) {
    this.videoElement_.play();
  } else {
    this.videoElement_.pause();
  }
};

/**
 * Plays the first video.
 */
NativeControlsVideoPlayer.prototype.playFirstVideo = function() {
  this.currentPos_ = 0;
  this.reloadCurrentVideo_(this.onFirstVideoReady_.wrap(this));
};

/**
 * Called when the first video is ready after starting to load.
 * Make the video player app window the same dimension as the video source
 * Restrict the app window inside the screen.
 *
 * @private
 */
NativeControlsVideoPlayer.prototype.onFirstVideoReady_ = function() {
  let videoWidth = this.videoElement_.videoWidth;
  let videoHeight = this.videoElement_.videoHeight;

  let aspect = videoWidth / videoHeight;
  let newWidth = videoWidth;
  let newHeight = videoHeight;

  let shrinkX = newWidth / window.screen.availWidth;
  let shrinkY = newHeight / window.screen.availHeight;
  if (shrinkX > 1 || shrinkY > 1) {
    if (shrinkY > shrinkX) {
      newHeight = newHeight / shrinkY;
      newWidth = newHeight * aspect;
    } else {
      newWidth = newWidth / shrinkX;
      newHeight = newWidth / aspect;
    }
  }

  let oldLeft = window.screenX;
  let oldTop = window.screenY;
  let oldWidth = window.innerWidth;
  let oldHeight = window.innerHeight;

  if (!oldWidth && !oldHeight) {
    oldLeft = window.screen.availWidth / 2;
    oldTop = window.screen.availHeight / 2;
  }

  let appWindow = chrome.app.window.current();
  appWindow.innerBounds.width = Math.round(newWidth);
  appWindow.innerBounds.height = Math.round(newHeight);
  appWindow.outerBounds.left =
      Math.max(0, Math.round(oldLeft - (newWidth - oldWidth) / 2));
  appWindow.outerBounds.top =
      Math.max(0, Math.round(oldTop - (newHeight - oldHeight) / 2));
  appWindow.show();

  this.videoElement_.focus();
  this.videoElement_.play();
};

/**
 * Advance to next video when the current one ends.
 * Not using 'ended' event because dragging the timeline
 * thumb to the end when seeking will trigger 'ended' event.
 *
 * @private
 */
NativeControlsVideoPlayer.prototype.onPause_ = function() {
  if (this.videoElement_.currentTime == this.videoElement_.duration) {
    this.advance_(true);
  }
};

/**
 * Advances to the next (or previous) track.
 *
 * @param {boolean} direction True to the next, false to the previous.
 * @private
 */
NativeControlsVideoPlayer.prototype.advance_ = function(direction) {
  let newPos = this.currentPos_ + (direction ? 1 : -1);
  if (newPos < 0 || newPos >= this.videos_.length) {
    return;
  }

  this.currentPos_ = newPos;
  this.reloadCurrentVideo_(function() {
    this.videoElement_.play();
  }.wrap(this));
};

/**
 * Reloads the current video.
 *
 * @param {function()=} opt_callback Completion callback.
 */
NativeControlsVideoPlayer.prototype.reloadCurrentVideo_ = function(
    opt_callback) {
  let videoPlayerElement = getRequiredElement('video-player');
  if (this.currentPos_ == (this.videos_.length - 1)) {
    videoPlayerElement.setAttribute('last-video', true);
  } else {
    videoPlayerElement.removeAttribute('last-video');
  }

  if (this.currentPos_ === 0) {
    videoPlayerElement.setAttribute('first-video', true);
  } else {
    videoPlayerElement.removeAttribute('first-video');
  }

  let currentVideo = this.videos_[this.currentPos_];
  this.loadVideo_(currentVideo, opt_callback);
};

/**
 * Loads the video file.
 * @param {!FileEntry} video Entry of the video to be played.
 * @param {function()=} opt_callback Completion callback.
 * @private
 */
NativeControlsVideoPlayer.prototype.loadVideo_ = function(video, opt_callback) {
  this.videoElement_.src = video.toURL();
  if (opt_callback) {
    this.videoElement_.addEventListener(
        'loadedmetadata', opt_callback, {once: true});
  }
};
