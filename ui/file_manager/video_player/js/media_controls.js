// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MediaControls class implements media playback controls
 * that exist outside of the audio/video HTML element.
 */

/**
 * @param {!HTMLElement} containerElement The container for the controls.
 * @param {function(Event)} onMediaError Function to display an error message.
 * @constructor
 * @struct
 */
function MediaControls(containerElement, onMediaError) {
  this.container_ = containerElement;
  this.document_ = this.container_.ownerDocument;
  this.media_ = null;

  this.onMediaPlayBound_ = this.onMediaPlay_.bind(this, true);
  this.onMediaPauseBound_ = this.onMediaPlay_.bind(this, false);
  this.onMediaDurationBound_ = this.onMediaDuration_.bind(this);
  this.onMediaProgressBound_ = this.onMediaProgress_.bind(this);
  this.onMediaError_ = onMediaError || function() {};

  this.savedVolume_ = 1;  // 100% volume.

  /**
   * @type {HTMLElement}
   * @private
   */
  this.playButton_ = null;

  /**
   * @type {PaperSliderElement}
   * @private
   */
  this.progressSlider_ = null;

  /**
   * @type {PaperSliderElement}
   * @private
   */
  this.volume_ = null;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.textBanner_ = null;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.soundButton_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.resumeAfterDrag_ = false;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.currentTime_ = null;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.currentTimeSpacer_ = null;

  /**
   * @private {boolean}
   */
  this.seeking_ = false;
}

/**
 * Button's state types. Values are used as CSS class names.
 * @enum {string}
 */
MediaControls.ButtonStateType = {
  DEFAULT: 'default',
  PLAYING: 'playing',
  ENDED: 'ended'
};

/**
 * @return {HTMLAudioElement|HTMLVideoElement} The media element.
 */
MediaControls.prototype.getMedia = function() { return this.media_ };

/**
 * Format the time in hh:mm:ss format (omitting redundant leading zeros).
 *
 * @param {number} timeInSec Time in seconds.
 * @return {string} Formatted time string.
 * @private
 */
MediaControls.formatTime_ = function(timeInSec) {
  var seconds = Math.floor(timeInSec % 60);
  var minutes = Math.floor((timeInSec / 60) % 60);
  var hours = Math.floor(timeInSec / 60 / 60);
  var result = '';
  if (hours) result += hours + ':';
  if (hours && (minutes < 10)) result += '0';
  result += minutes + ':';
  if (seconds < 10) result += '0';
  result += seconds;
  return result;
};

/**
 * Create a custom control.
 *
 * @param {string} className Class name.
 * @param {HTMLElement=} opt_parent Parent element or container if undefined.
 * @param {string=} opt_tagName Tag name of the control. 'div' if undefined.
 * @return {!HTMLElement} The new control element.
 */
MediaControls.prototype.createControl =
    function(className, opt_parent, opt_tagName) {
  var parent = opt_parent || this.container_;
  var control = /** @type {!HTMLElement} */
      (this.document_.createElement(opt_tagName || 'div'));
  control.className = className;
  parent.appendChild(control);
  return control;
};

/**
 * Create a custom button.
 *
 * @param {string} className Class name.
 * @param {function(Event)=} opt_handler Click handler.
 * @param {HTMLElement=} opt_parent Parent element or container if undefined.
 * @param {number=} opt_numStates Number of states, default: 1.
 * @return {!HTMLElement} The new button element.
 */
MediaControls.prototype.createButton = function(
    className, opt_handler, opt_parent, opt_numStates) {
  opt_numStates = opt_numStates || 1;

  var button = this.createControl(className, opt_parent, 'files-icon-button');
  button.classList.add('media-button');

  button.setAttribute('state', MediaControls.ButtonStateType.DEFAULT);

  if (opt_handler)
    button.addEventListener('click', opt_handler);

  return button;
};

/**
 * Enable/disable controls.
 *
 * @param {boolean} on True if enable, false if disable.
 * @private
 */
MediaControls.prototype.enableControls_ = function(on) {
  var controls = this.container_.querySelectorAll('.media-control');
  for (var i = 0; i != controls.length; i++) {
    var classList = controls[i].classList;
    if (on)
      classList.remove('disabled');
    else
      classList.add('disabled');
  }
  this.progressSlider_.disabled = !on;
  this.volume_.disabled = !on;
};

/*
 * Playback control.
 */

/**
 * Play the media.
 */
MediaControls.prototype.play = function() {
  if (!this.media_)
    return;  // Media is detached.

  this.media_.play();
};

/**
 * Pause the media.
 */
MediaControls.prototype.pause = function() {
  if (!this.media_)
    return;  // Media is detached.

  this.media_.pause();
};

/**
 * @return {boolean} True if the media is currently playing.
 */
MediaControls.prototype.isPlaying = function() {
  return !!this.media_ && !this.media_.paused && !this.media_.ended;
};

/**
 * Toggle play/pause.
 */
MediaControls.prototype.togglePlayState = function() {
  if (this.isPlaying())
    this.pause();
  else
    this.play();
};

/**
 * Toggles play/pause state on a mouse click on the play/pause button.
 *
 * @param {Event} event Mouse click event.
 */
MediaControls.prototype.onPlayButtonClicked = function(event) {
  this.togglePlayState();
};

/**
 * @param {HTMLElement=} opt_parent Parent container.
 */
MediaControls.prototype.initPlayButton = function(opt_parent) {
  this.playButton_ = this.createButton('play media-control',
      this.onPlayButtonClicked.bind(this), opt_parent, 3 /* States. */);
  this.playButton_.setAttribute('aria-label',
      str('MEDIA_PLAYER_PLAY_BUTTON_LABEL'));
};

/*
 * Time controls
 */

/**
 * The default range of 100 is too coarse for the media progress slider.
 */
MediaControls.PROGRESS_RANGE = 5000;

/**
 * 5 seconds should be skipped when left/right key is pressed on progress bar.
 */
MediaControls.PROGRESS_MAX_SECONDS_TO_SKIP = 5;

/**
 * 10% of duration should be skipped when the video is too short to skip 5
 * seconds.
 */
MediaControls.PROGRESS_MAX_RATIO_TO_SKIP = 0.1;

/**
 * @param {HTMLElement=} opt_parent Parent container.
 */
MediaControls.prototype.initTimeControls = function(opt_parent) {
  var timeControls = this.createControl('time-controls', opt_parent);

  var timeBox = this.createControl('time media-control', timeControls);

  this.currentTimeSpacer_ = this.createControl('spacer', timeBox);
  this.currentTime_ = this.createControl('current', timeBox);
  // Set the initial width to the minimum to reduce the flicker.
  this.updateTimeLabel_(0, 0);

  this.progressSlider_ = /** @type {!PaperSliderElement} */ (
      document.createElement('paper-slider'));
  this.progressSlider_.classList.add('progress', 'media-control');
  this.progressSlider_.max = MediaControls.PROGRESS_RANGE;
  this.progressSlider_.setAttribute('aria-label',
      str('MEDIA_PLAYER_SEEK_SLIDER_LABEL'));
  this.progressSlider_.addEventListener('change', function(event) {
    this.onProgressChange_(this.progressSlider_.ratio);
  }.bind(this));
  this.progressSlider_.addEventListener(
      'immediate-value-change',
      function(event) {
        this.onProgressDrag_();
      }.bind(this));
  this.progressSlider_.addEventListener('keydown',
      this.onProgressKeyDownOrKeyPress_.bind(this));
  this.progressSlider_.addEventListener('keypress',
      this.onProgressKeyDownOrKeyPress_.bind(this));
  timeControls.appendChild(this.progressSlider_);
};

/**
 * @param {number} current Current time is seconds.
 * @param {number} duration Duration in seconds.
 * @private
 */
MediaControls.prototype.displayProgress_ = function(current, duration) {
  var ratio = current / duration;
  this.progressSlider_.value = ratio * this.progressSlider_.max;
  this.updateTimeLabel_(current);
};

/**
 * @param {number} value Progress [0..1].
 * @private
 */
MediaControls.prototype.onProgressChange_ = function(value) {
  if (!this.media_)
    return;  // Media is detached.

  if (!this.media_.seekable || !this.media_.duration) {
    console.error('Inconsistent media state');
    return;
  }

  this.setSeeking_(false);

  var current = this.media_.duration * value;
  this.media_.currentTime = current;
  this.updateTimeLabel_(current);
};

/**
 * @private
 */
MediaControls.prototype.onProgressDrag_ = function() {
  if (!this.media_)
    return;  // Media is detached.

  this.setSeeking_(true);

  // Show seeking position instead of playing position while dragging.
  if (this.media_.duration && this.progressSlider_.max > 0) {
    var immediateRatio =
        this.progressSlider_.immediateValue / this.progressSlider_.max;
    var current = this.media_.duration * immediateRatio;
    this.updateTimeLabel_(current);
  }
};

/**
 * Handles arrow keys on progress slider to skip forward/backword.
 * @param {!Event} event
 * @private
 */
MediaControls.prototype.onProgressKeyDownOrKeyPress_ = function(event) {
  if (event.code !== 'ArrowRight' && event.code !== 'ArrowLeft' &&
      event.code !== 'ArrowUp' && event.code !== 'ArrowDown') {
    return;
  }

  event.preventDefault();

  if (this.media_ && this.media_.duration > 0) {
    // Skip 5 seconds or 10% of duration, whichever is smaller.
    var secondsToSkip = Math.min(
        MediaControls.PROGRESS_MAX_SECONDS_TO_SKIP,
        this.media_.duration * MediaControls.PROGRESS_MAX_RATIO_TO_SKIP);
    var stepsToSkip = MediaControls.PROGRESS_RANGE *
        (secondsToSkip / this.media_.duration);

    if (event.code === 'ArrowRight' || event.code === 'ArrowUp') {
      this.progressSlider_.value = Math.min(
          this.progressSlider_.value + stepsToSkip,
          this.progressSlider_.max);
    } else {
      this.progressSlider_.value = Math.max(
          this.progressSlider_.value - stepsToSkip, 0);
    }
    this.onProgressChange_(this.progressSlider_.ratio);
  }
};

/**
 * Handles 'seeking' state, which starts by dragging slider knob and finishes by
 * releasing it. While seeking, we pause the video when seeking starts and
 * resume the last play state when seeking ends.
 * @private
 */
MediaControls.prototype.setSeeking_ = function(seeking) {
  if (seeking === this.seeking_)
    return;

  this.seeking_ = seeking;

  if (seeking) {
    this.resumeAfterDrag_ = this.isPlaying();
    this.media_.pause(true /* seeking */);
  } else {
    if (this.resumeAfterDrag_) {
      if (this.media_.ended)
        this.onMediaPlay_(false);
      else
        this.media_.play(true /* seeking */);
    }
    this.resumeAfterDrag_ = false;
  }
  this.updatePlayButtonState_(this.isPlaying());
};


/**
 * Update the label for current playing position and video duration.
 * The label should be like "0:00 / 6:20".
 * @param {number} current Current playing position.
 * @param {number=} opt_duration Video's duration.
 * @private
 */
MediaControls.prototype.updateTimeLabel_ = function(current, opt_duration) {
  var duration = opt_duration;
  if (duration === undefined)
    duration = this.media_ ? this.media_.duration : 0;
  // media's duration and currentTime can be NaN. Default to 0.
  if (isNaN(duration))
    duration = 0;
  if (isNaN(current))
    current = 0;

  if (isFinite(duration)) {
    this.currentTime_.textContent =
        MediaControls.formatTime_(current) + ' / ' +
        MediaControls.formatTime_(duration);
    // Keep the maximum space to prevent time label from moving while playing.
    this.currentTimeSpacer_.textContent =
        MediaControls.formatTime_(duration) + ' / ' +
        MediaControls.formatTime_(duration);
  } else {
    // Media's duration can be positive infinity value when the media source is
    // not known to be bounded yet. In such cases, we should hide duration.
    this.currentTime_.textContent = MediaControls.formatTime_(current);
    this.currentTimeSpacer_.textContent = MediaControls.formatTime_(current);
  }
};

/*
 * Volume controls
 */

/**
 * @param {HTMLElement=} opt_parent Parent element for the controls.
 */
MediaControls.prototype.initVolumeControls = function(opt_parent) {
  var volumeControls = this.createControl('volume-controls', opt_parent);

  this.soundButton_ = this.createButton('sound media-control',
      this.onSoundButtonClick_.bind(this), volumeControls);
  this.soundButton_.setAttribute('level', 3);  // max level.
  this.soundButton_.setAttribute('aria-label',
      str('MEDIA_PLAYER_MUTE_BUTTON_LABEL'));

  this.volume_ = /** @type {!PaperSliderElement} */ (
      document.createElement('paper-slider'));
  this.volume_.classList.add('volume', 'media-control');
  this.volume_.setAttribute('aria-label',
      str('MEDIA_PLAYER_VOLUME_SLIDER_LABEL'));
  this.volume_.addEventListener('change', function(event) {
    this.onVolumeChange_(this.volume_.ratio);
  }.bind(this));
  this.volume_.addEventListener('immediate-value-change', function(event) {
    this.onVolumeDrag_();
  }.bind(this));
  this.volume_.value = this.volume_.max;
  volumeControls.appendChild(this.volume_);
};

/**
 * Click handler for the sound level button.
 * @private
 */
MediaControls.prototype.onSoundButtonClick_ = function() {
  if (this.media_.volume == 0) {
    this.volume_.value = (this.savedVolume_ || 1) * this.volume_.max;
    this.soundButton_.setAttribute('aria-label',
        str('MEDIA_PLAYER_MUTE_BUTTON_LABEL'));
  } else {
    this.savedVolume_ = this.media_.volume;
    this.volume_.value = 0;
    this.soundButton_.setAttribute('aria-label',
        str('MEDIA_PLAYER_UNMUTE_BUTTON_LABEL'));
  }
  this.onVolumeChange_(this.volume_.ratio);
};

/**
 * @param {number} value Volume [0..1].
 * @return {number} The rough level [0..3] used to pick an icon.
 * @private
 */
MediaControls.getVolumeLevel_ = function(value) {
  if (value == 0) return 0;
  if (value <= 1 / 3) return 1;
  if (value <= 2 / 3) return 2;
  return 3;
};

/**
 * @param {number} value Volume [0..1].
 * @private
 */
MediaControls.prototype.onVolumeChange_ = function(value) {
  if (!this.media_)
    return;  // Media is detached.

  this.media_.volume = value;
  this.soundButton_.setAttribute('level', MediaControls.getVolumeLevel_(value));
  this.soundButton_.setAttribute('aria-label',
      value === 0 ? str('MEDIA_PLAYER_UNMUTE_BUTTON_LABEL')
                  : str('MEDIA_PLAYER_MUTE_BUTTON_LABEL'));
};

/**
 * @private
 */
MediaControls.prototype.onVolumeDrag_ = function() {
  if (this.media_.volume !== 0) {
    this.savedVolume_ = this.media_.volume;
  }
};

/*
 * Media event handlers.
 */

/**
 * Attach a media element.
 *
 * @param {!HTMLMediaElement} mediaElement The media element to control.
 */
MediaControls.prototype.attachMedia = function(mediaElement) {
  this.media_ = mediaElement;

  this.media_.addEventListener('play', this.onMediaPlayBound_);
  this.media_.addEventListener('pause', this.onMediaPauseBound_);
  this.media_.addEventListener('durationchange', this.onMediaDurationBound_);
  this.media_.addEventListener('timeupdate', this.onMediaProgressBound_);
  this.media_.addEventListener('error', this.onMediaError_);

  // If the text banner is being displayed, hide it immediately, since it is
  // related to the previous media.
  this.textBanner_.removeAttribute('visible');

  // Reflect the media state in the UI.
  this.onMediaDuration_();
  this.onMediaPlay_(this.isPlaying());
  this.onMediaProgress_();
  if (this.volume_) {
    /* Copy the user selected volume to the new media element. */
    this.savedVolume_ = this.media_.volume = this.volume_.ratio;
  }
};

/**
 * Detach media event handlers.
 */
MediaControls.prototype.detachMedia = function() {
  if (!this.media_)
    return;

  this.media_.removeEventListener('play', this.onMediaPlayBound_);
  this.media_.removeEventListener('pause', this.onMediaPauseBound_);
  this.media_.removeEventListener('durationchange', this.onMediaDurationBound_);
  this.media_.removeEventListener('timeupdate', this.onMediaProgressBound_);
  this.media_.removeEventListener('error', this.onMediaError_);

  this.media_ = null;
};

/**
 * Force-empty the media pipeline. This is a workaround for crbug.com/149957.
 * The document is not going to be GC-ed until the last Files app window closes,
 * but we want the media pipeline to deinitialize ASAP to minimize leakage.
 */
MediaControls.prototype.cleanup = function() {
  if (!this.media_)
    return;

  this.media_.src = '';
  this.media_.load();
  this.detachMedia();
};

/**
 * 'play' and 'pause' event handler.
 * @param {boolean} playing True if playing.
 * @private
 */
MediaControls.prototype.onMediaPlay_ = function(playing) {
  if (this.progressSlider_.dragging)
    return;

  this.updatePlayButtonState_(playing);
  this.onPlayStateChanged();
};

/**
 * 'durationchange' event handler.
 * @private
 */
MediaControls.prototype.onMediaDuration_ = function() {
  if (!this.media_ || !this.media_.duration) {
    this.enableControls_(false);
    return;
  }

  this.enableControls_(true);

  if (this.media_.seekable)
    this.progressSlider_.classList.remove('readonly');
  else
    this.progressSlider_.classList.add('readonly');

  this.updateTimeLabel_(this.media_.currentTime, this.media_.duration);

  if (this.media_.seekable)
    this.restorePlayState();
};

/**
 * 'timeupdate' event handler.
 * @private
 */
MediaControls.prototype.onMediaProgress_ = function() {
  if (!this.media_ || !this.media_.duration) {
    this.displayProgress_(0, 1);
    return;
  }

  var current = this.media_.currentTime;
  var duration = this.media_.duration;

  if (this.progressSlider_.dragging)
    return;

  this.displayProgress_(current, duration);

  if (current == duration) {
    this.onMediaComplete();
  }
  this.onPlayStateChanged();
};

/**
 * Called when the media playback is complete.
 */
MediaControls.prototype.onMediaComplete = function() {};

/**
 * Called when play/pause state is changed or on playback progress.
 * This is the right moment to save the play state.
 */
MediaControls.prototype.onPlayStateChanged = function() {};

/**
 * Updates the play button state.
 * @param {boolean} playing If the video is playing.
 * @private
 */
MediaControls.prototype.updatePlayButtonState_ = function(playing) {
  if (this.media_.ended &&
      this.progressSlider_.value === this.progressSlider_.max) {
    this.playButton_.setAttribute('state',
                                  MediaControls.ButtonStateType.ENDED);
    this.playButton_.setAttribute('aria-label',
        str('MEDIA_PLAYER_PLAY_BUTTON_LABEL'));
  } else if (playing) {
    this.playButton_.setAttribute('state',
                                  MediaControls.ButtonStateType.PLAYING);
    this.playButton_.setAttribute('aria-label',
        str('MEDIA_PLAYER_PAUSE_BUTTON_LABEL'));
  } else {
    this.playButton_.setAttribute('state',
                                  MediaControls.ButtonStateType.DEFAULT);
    this.playButton_.setAttribute('aria-label',
        str('MEDIA_PLAYER_PLAY_BUTTON_LABEL'));
  }
};

/**
 * Restore play state. Base implementation is empty.
 */
MediaControls.prototype.restorePlayState = function() {};

/**
 * Encode current state into the page URL or the app state.
 */
MediaControls.prototype.encodeState = function() {
  if (!this.media_ || !this.media_.duration)
    return;

  if (window.appState) {
    window.appState.time = this.media_.currentTime;
    util.saveAppState();
  }
  return;
};

/**
 * Decode current state from the page URL or the app state.
 * @return {boolean} True if decode succeeded.
 */
MediaControls.prototype.decodeState = function() {
  if (!this.media_ || !window.appState || !('time' in window.appState))
    return false;
  // There is no page reload for apps v2, only app restart.
  // Always restart in paused state.
  this.media_.currentTime = window.appState.time;
  this.pause();
  return true;
};

/**
 * Remove current state from the page URL or the app state.
 */
MediaControls.prototype.clearState = function() {
  if (!window.appState)
    return;

  if ('time' in window.appState)
    delete window.appState.time;
  util.saveAppState();
  return;
};

/**
 * Create video controls.
 *
 * @param {!HTMLElement} containerElement The container for the controls.
 * @param {function(Event)} onMediaError Function to display an error message.
 * @param {function(Event)=} opt_fullScreenToggle Function to toggle fullscreen
 *     mode.
 * @param {HTMLElement=} opt_stateIconParent The parent for the icon that
 *     gives visual feedback when the playback state changes.
 * @constructor
 * @struct
 * @extends {MediaControls}
 */
function VideoControls(
    containerElement, onMediaError, opt_fullScreenToggle, opt_stateIconParent) {
  MediaControls.call(this, containerElement, onMediaError);

  this.container_.classList.add('video-controls');
  this.initPlayButton();
  this.initTimeControls();
  this.initVolumeControls();

  // Create the cast button.
  // We need to use <button> since cr.ui.MenuButton.decorate modifies prototype
  // chain, by which <files-icon-button> will not work correctly.
  // TODO(fukino): Find a way to use files-icon-button consistently.
  this.castButton_ = this.createControl(
      'cast media-button', undefined, 'button');
  this.castButton_.setAttribute('menu', '#cast-menu');
  this.castButton_.setAttribute('aria-label', str('VIDEO_PLAYER_PLAY_ON'));
  this.castButton_.setAttribute('state', MediaControls.ButtonStateType.DEFAULT);
  this.castButton_.appendChild(document.createElement('files-ripple'));
  cr.ui.decorate(this.castButton_, cr.ui.MenuButton);

  if (opt_fullScreenToggle) {
    this.fullscreenButton_ =
        this.createButton('fullscreen', opt_fullScreenToggle);
    this.fullscreenButton_.setAttribute('aria-label',
        str('VIDEO_PLAYER_FULL_SCREEN_BUTTON_LABEL'));
  }

  if (opt_stateIconParent) {
    this.stateIcon_ = this.createControl(
        'playback-state-icon', opt_stateIconParent);
    this.textBanner_ = this.createControl('text-banner', opt_stateIconParent);
  }

  // Disables all controls at first.
  this.enableControls_(false);

  var videoControls = this;
  chrome.mediaPlayerPrivate.onTogglePlayState.addListener(
      function() { videoControls.togglePlayStateWithFeedback(); });
}

/**
 * No resume if we are within this margin from the start or the end.
 */
VideoControls.RESUME_MARGIN = 0.03;

/**
 * No resume for videos shorter than this.
 */
VideoControls.RESUME_THRESHOLD = 5 * 60; // 5 min.

/**
 * When resuming rewind back this much.
 */
VideoControls.RESUME_REWIND = 5;  // seconds.

VideoControls.prototype = { __proto__: MediaControls.prototype };

/**
 * Shows icon feedback for the current state of the video player.
 * @private
 */
VideoControls.prototype.showIconFeedback_ = function() {
  var stateIcon = this.stateIcon_;
  stateIcon.removeAttribute('state');

  setTimeout(function() {
    var newState = this.isPlaying() ? 'play' : 'pause';

    var onAnimationEnd = function(state, event) {
      if (stateIcon.getAttribute('state') === state)
        stateIcon.removeAttribute('state');

      stateIcon.removeEventListener('webkitAnimationEnd', onAnimationEnd);
    }.bind(null, newState);
    stateIcon.addEventListener('webkitAnimationEnd', onAnimationEnd);

    // Shows the icon with animation.
    stateIcon.setAttribute('state', newState);
  }.bind(this), 0);
};

/**
 * Shows a text banner.
 *
 * @param {string} identifier String identifier.
 * @private
 */
VideoControls.prototype.showTextBanner_ = function(identifier) {
  this.textBanner_.removeAttribute('visible');
  this.textBanner_.textContent = str(identifier);

  setTimeout(function() {
    var onAnimationEnd = function(event) {
      this.textBanner_.removeEventListener(
          'webkitAnimationEnd', onAnimationEnd);
      this.textBanner_.removeAttribute('visible');
    }.bind(this);
    this.textBanner_.addEventListener('webkitAnimationEnd', onAnimationEnd);

    this.textBanner_.setAttribute('visible', 'true');
  }.bind(this), 0);
};

/**
 * @override
 */
VideoControls.prototype.onPlayButtonClicked = function(event) {
  if (event.ctrlKey) {
    this.toggleLoopedModeWithFeedback(true);
    if (!this.isPlaying())
      this.togglePlayState();
  } else {
    this.togglePlayState();
  }
};

/**
 * Media completion handler.
 */
VideoControls.prototype.onMediaComplete = function() {
  this.onMediaPlay_(false);  // Just update the UI.
  this.savePosition();  // This will effectively forget the position.
};

/**
 * Toggles the looped mode with feedback.
 * @param {boolean} on Whether enabled or not.
 */
VideoControls.prototype.toggleLoopedModeWithFeedback = function(on) {
  if (!this.getMedia().duration)
    return;
  this.toggleLoopedMode(on);
  if (on) {
    // TODO(mtomasz): Simplify, crbug.com/254318.
    this.showTextBanner_('VIDEO_PLAYER_LOOPED_MODE');
  }
};

/**
 * Toggles the looped mode.
 * @param {boolean} on Whether enabled or not.
 */
VideoControls.prototype.toggleLoopedMode = function(on) {
  this.getMedia().loop = on;
};

/**
 * Toggles play/pause state and flash an icon over the video.
 */
VideoControls.prototype.togglePlayStateWithFeedback = function() {
  if (!this.getMedia().duration)
    return;

  this.togglePlayState();
  this.showIconFeedback_();
};

/**
 * Toggles play/pause state.
 */
VideoControls.prototype.togglePlayState = function() {
  if (this.isPlaying()) {
    // User gave the Pause command. Save the state and reset the loop mode.
    this.toggleLoopedMode(false);
    this.savePosition();
  }
  MediaControls.prototype.togglePlayState.apply(this, arguments);
};

/**
 * Saves the playback position to the persistent storage.
 * @param {boolean=} opt_sync True if the position must be saved synchronously
 *     (required when closing app windows).
 */
VideoControls.prototype.savePosition = function(opt_sync) {
  if (!this.media_ ||
      !this.media_.duration ||
      this.media_.duration < VideoControls.RESUME_THRESHOLD) {
    return;
  }

  var ratio = this.media_.currentTime / this.media_.duration;
  var position;
  if (ratio < VideoControls.RESUME_MARGIN ||
      ratio > (1 - VideoControls.RESUME_MARGIN)) {
    // We are too close to the beginning or the end.
    // Remove the resume position so that next time we start from the beginning.
    position = null;
  } else {
    position = Math.floor(
        Math.max(0, this.media_.currentTime - VideoControls.RESUME_REWIND));
  }

  if (opt_sync) {
    // Packaged apps cannot save synchronously.
    // Pass the data to the background page.
    if (!window.saveOnExit)
      window.saveOnExit = [];
    window.saveOnExit.push({ key: this.media_.src, value: position });
  } else {
    util.AppCache.update(this.media_.src, position);
  }
};

/**
 * Resumes the playback position saved in the persistent storage.
 */
VideoControls.prototype.restorePlayState = function() {
  if (this.media_ && this.media_.duration >= VideoControls.RESUME_THRESHOLD) {
    util.AppCache.getValue(this.media_.src, function(position) {
      if (position)
        this.media_.currentTime = position;
    }.bind(this));
  }
};

/**
 * Updates video control when the window is fullscreened or restored.
 * @param {boolean} fullscreen True if the window gets fullscreened.
 */
VideoControls.prototype.onFullScreenChanged = function(fullscreen) {
  if (fullscreen) {
    this.container_.setAttribute('fullscreen', '');
  } else {
    this.container_.removeAttribute('fullscreen');
  }

  if (this.fullscreenButton_) {
    this.fullscreenButton_.setAttribute('aria-label',
        fullscreen ? str('VIDEO_PLAYER_EXIT_FULL_SCREEN_BUTTON_LABEL')
                   : str('VIDEO_PLAYER_FULL_SCREEN_BUTTON_LABEL'));;
  }
};
