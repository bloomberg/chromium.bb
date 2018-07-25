// Copyright 2018 The Chromium OS Authors. All rights reserved./
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Namespace for Camera view.
 */
camera.views.camera = camera.views.camera || {};

/**
 * Creates a controller for the options of Camera view.
 * @param {camera.Router} router View router to switch views.
 * @param {function()} onNewStreamNeeded Callback called when new stream is
 *     needed for the changed options.
 * @constructor
 */
camera.views.camera.Options = function(router, onNewStreamNeeded) {
  /**
   * @type {camera.Router}
   * @private
   */
  this.router_ = router;

  /**
   * @type {function()}
   * @private
   */
  this.onNewStreamNeeded_ = onNewStreamNeeded;

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleMirror_ = document.querySelector('#toggle-mirror');

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleTimer_ = document.querySelector('#toggle-timer');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.toggleDevice_ = document.querySelector('#toggle-device');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.switchRecordVideo_ = document.querySelector('#switch-recordvideo');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.switchTakePhoto_ = document.querySelector('#switch-takephoto');

  /**
   * @type {Audio}
   * @private
   */
  this.shutterSound_ = document.createElement('audio');

  /**
   * @type {Audio}
   * @private
   */
  this.tickSound_ = document.createElement('audio');

  /**
   * @type {Audio}
   * @private
   */
  this.recordStartSound_ = document.createElement('audio');

  /**
   * @type {Audio}
   * @private
   */
  this.recordEndSound_ = document.createElement('audio');

  /**
   * Device id of the camera device currently used or selected.
   * @type {?string}
   * @private
   */
  this.videoDeviceId_ = null;

  /**
   * Whether list of video devices is being refreshed now.
   * @type {boolean}
   * @private
   */
  this.refreshingVideoDeviceIds_ = false;

  /**
   * List of available video device ids.
   * @type {Promise<!Array<string>>}
   * @private
   */
  this.videoDeviceIds_ = null;

  /**
   * Mirroring set per device.
   * @type {Object}
   * @private
   */
  this.mirroringToggles_ = {};

  /**
   * Timeout to countdown a tick of the timer.
   * @type {?number}
   * @private
   */
  this.tickTimeout_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  this.switchRecordVideo_.addEventListener(
      'click', this.onSwitchRecordVideoClicked_.bind(this));
  this.switchTakePhoto_.addEventListener(
      'click', this.onSwitchTakePhotoClicked_.bind(this));
  this.toggleDevice_.addEventListener(
      'click', this.onToggleDeviceClicked_.bind(this));
  this.toggleMirror_.addEventListener(
      'click', this.onToggleMirrorClicked_.bind(this));
  this.toggleTimer_.addEventListener(
      'click', this.onToggleTimerClicked_.bind(this));

  // Handle key-press for checkbox-type toggles.
  [this.toggleMirror_, this.toggleTimer_].forEach(element => {
    element.addEventListener('keypress', event => {
      if (camera.util.getShortcutIdentifier(event) == 'Enter') {
        element.click();
      }
    });
  });

  // Load the shutter, tick, and recording sound.
  this.shutterSound_.src = '../sounds/shutter.ogg';
  this.tickSound_.src = '../sounds/tick.ogg';
  this.recordStartSound_.src = '../sounds/record_start.ogg';
  this.recordEndSound_.src = '../sounds/record_end.ogg';
};

/**
 * Sounds.
 * @enum {number}
 */
camera.views.camera.Options.Sound = Object.freeze({
  SHUTTER: 0,
  TICK: 1,
  RECORDSTART: 2,
  RECORDEND: 3
});

camera.views.camera.Options.prototype = {
  get recordMode() {
    return document.body.classList.contains('record-mode');
  },
  set disabled(value) {
    this.switchRecordVideo_.disabled = value;
    this.switchTakePhoto_.disabled = value;
    this.toggleDevice_.disabled = value;
    this.toggleMirror_.disabled = value;
    this.toggleTimer_.disabled = value;
  }
};

/**
 * Prepares the options.
 */
camera.views.camera.Options.prototype.prepare = function() {
  // Show the mirror toggle on flipping-camera chromebooks (crbug.com/847737).
  camera.util.isChromeOSDevice(
      ['LENOVO-REKS1', 'LENOVO-REKS3', 'LENOVO-REKS4']).then(result => {
    this.toggleMirror_.hidden = !result;
  });

  // Select the default state of the toggle buttons.
  chrome.storage.local.get({
    toggleTimer: false,
    mirroringToggles: {},  // Manually mirroring states per video device.
  }, values => {
    this.toggleTimer_.checked = values.toggleTimer;
    this.mirroringToggles_ = values.mirroringToggles;
  });
  // Remove the deprecated values.
  chrome.storage.local.remove(['effectIndex', 'toggleMulti', 'toggleMirror']);

  // TODO(yuli): Replace with devicechanged event.
  this.maybeRefreshVideoDeviceIds_();
  setInterval(this.maybeRefreshVideoDeviceIds_.bind(this), 1000);
};


/**
 * Handles clicking on the video-recording switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onSwitchRecordVideoClicked_ = function(
    event) {
  document.body.classList.add('record-mode');
  this.onNewStreamNeeded_();
};

/**
 * Handles clicking on the photo-taking switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onSwitchTakePhotoClicked_ = function(
    event) {
  document.body.classList.remove('record-mode');
  this.onNewStreamNeeded_();
};

/**
 * Handles clicking on the toggle camera device switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleDeviceClicked_ = function(event) {
  this.videoDeviceIds_.then(deviceIds => {
    var index = deviceIds.indexOf(this.videoDeviceId_);
    if (index == -1) {
      index = 0;
    }
    if (deviceIds.length > 0) {
      index = (index + 1) % deviceIds.length;
      this.videoDeviceId_ = deviceIds[index];
    }
    this.onNewStreamNeeded_();
  });
};

/**
 * Handles clicking on the timer switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleTimerClicked_ = function(event) {
  chrome.storage.local.set({toggleTimer: this.toggleTimer_.checked});
};

/**
 * Handles clicking on the mirror switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.camera.Options.prototype.onToggleMirrorClicked_ = function(event) {
  var enabled = this.toggleMirror_.checked;
  this.mirroringToggles_[this.videoDeviceId_] = enabled;
  chrome.storage.local.set({mirroringToggles: this.mirroringToggles_});
  document.body.classList.toggle('mirror', enabled);
};

/**
 * Handles playing the given sound by the speaker option.
 * @param {camera.views.camera.Options.Sound} sound Sound to be played.
 * @return {boolean} Whether the sound being played.
 */
camera.views.camera.Options.prototype.onSound = function(sound) {
  // TODO(yuli): Don't play sounds if the speaker settings is muted.
  var play = (element) => {
    element.currentTime = 0;
    element.play();
    return true;
  };
  switch (sound) {
    case camera.views.camera.Options.Sound.SHUTTER:
      return play(this.shutterSound_);
    case camera.views.camera.Options.Sound.TICK:
      return play(this.tickSound_);
    case camera.views.camera.Options.Sound.RECORDSTART:
      return play(this.recordStartSound_);
    case camera.views.camera.Options.Sound.RECORDEND:
      return play(this.recordEndSound_);
  }
};

/**
 * Schedules a take of photo or recording by the timer option.
 * @param {function(boolean)} scheduled A take scheduled after the timer ticks.
 */
camera.views.camera.Options.prototype.onTimerTicks = function(scheduled) {
  var tickCounter = this.toggleTimer_.checked ? 10 : 0;
  var onTimerTick = () => {
    if (tickCounter == 0) {
      scheduled(this.recordMode);
    } else {
      tickCounter--;
      this.tickTimeout_ = setTimeout(onTimerTick, 1000);
      this.onSound(camera.views.camera.Options.Sound.TICK);
      // Blink the toggle timer button.
      this.toggleTimer_.classList.add('animate');
      setTimeout(() => {
        this.toggleTimer_.classList.remove('animate');
      }, 500);
    }
  };
  // First tick immediately in the next message loop cycle.
  this.tickTimeout_ = setTimeout(onTimerTick, 0);
};

/**
 * Cancels the pending timer ticks if any.
 */
camera.views.camera.Options.prototype.cancelTimerTicks = function() {
  if (this.tickTimeout_) {
    clearTimeout(this.tickTimeout_);
    this.tickTimeout_ = null;
  }
  this.toggleTimer_.classList.remove('animate');
};

/**
 * Updates the options related to the stream.
 * @param {Object} constraints Current stream constraints in use.
 * @param {MediaStream} stream Current Stream in use.
 */
camera.views.camera.Options.prototype.updateStreamOptions = function(
    constraints, stream) {
  this.updateVideoDeviceId_(constraints, stream);
  this.updateMirroring_(stream);
  if (this.recordMode) {
    // TODO(yuli): Disable capturing audio by the microphone option.
    var track = stream && stream.getAudioTracks()[0];
    if (track) {
      track.enabled = true;
    }
  }
};

/**
 * Updates the video device id by the stream constraints or the stream.
 * @param {Object} constraints Current stream constraints in use.
 * @param {MediaStream} stream Current Stream in use.
 * @private
 */
camera.views.camera.Options.prototype.updateVideoDeviceId_ = function(
    constraints, stream) {
  if (constraints.video.deviceId) {
    // For non-default cameras fetch the deviceId from constraints.
    // Works on all supported Chrome versions.
    this.videoDeviceId_ = constraints.video.deviceId.exact;
  } else {
    // For default camera, obtain the deviceId from settings, which is
    // a feature available only from 59. For older Chrome versions,
    // it's impossible to detect the device id. As a result, if the
    // default camera was changed to rear in chrome://settings, then
    // toggling the camera may not work when pressed for the first time
    // (the same camera would be opened).
    var track = stream.getVideoTracks()[0];
    var trackSettings = track.getSettings && track.getSettings();
    this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
  }
};

/**
 * Updates mirroring for a new stream.
 * @param {MediaStream} stream Current Stream in use.
 * @private
 */
camera.views.camera.Options.prototype.updateMirroring_ = function(stream) {
  // Update mirroring by detected facing-mode. Enable mirroring by default if
  // facing-mode isn't available.
  var track = stream && stream.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  var facingMode = trackSettings && trackSettings.facingMode;
  var enabled = facingMode ? facingMode == 'user' : true;

  if (this.toggleMirror_.hidden) {
    document.body.classList.toggle('mirror', enabled);
  } else {
    // Override mirroring if it was set manually by the mirror toggle.
    if (this.videoDeviceId_ in this.mirroringToggles_) {
      enabled = this.mirroringToggles_[this.videoDeviceId_];
    }
    if (this.toggleMirror_.checked != enabled) {
      this.toggleMirror_.click();
    }
  }
};

/**
 * Updates list of available video devices when changed, including the UI.
 * Does nothing if refreshing is already in progress.
 * @private
 */
camera.views.camera.Options.prototype.maybeRefreshVideoDeviceIds_ = function() {
  if (this.refreshingVideoDeviceIds_) {
    return;
  }
  this.refreshingVideoDeviceIds_ = true;

  this.videoDeviceIds_ =
      navigator.mediaDevices.enumerateDevices().then(devices => {
    var availableVideoDevices = [];
    devices.forEach(device => {
      if (device.kind == 'videoinput') {
        availableVideoDevices.push(device.deviceId);
      }
    });
    return availableVideoDevices;
  });

  // Update UI options.
  this.videoDeviceIds_.then(deviceIds => {
    this.toggleDevice_.hidden = deviceIds.length < 2;
  }).catch(error => {
    console.error(error);
    this.toggleDevice_.hidden = true;
  }).finally(() => {
    this.refreshingVideoDeviceIds_ = false;
  });
};

/**
 * Gets the video device ids sorted by preference.
 * @return {!Promise<!Array<string>}
 */
camera.views.camera.Options.prototype.videoDeviceIds = function() {
  return this.videoDeviceIds_.then(deviceIds => {
    if (deviceIds.length == 0) {
      throw 'Device list empty.';
    }
    // Put the selected video device id first.
    var sorted = deviceIds.slice(0).sort((a, b) => {
      if (a == b) {
        return 0;
      }
      if (a == this.videoDeviceId_) {
        return -1;
      }
      return 1;
    });

    // Prepended 'null' deviceId means the system default camera. Add it only
    // when the app is launched (no video-device-id set).
    if (this.videoDeviceId_ == null) {
      sorted.unshift(null);
    }
    return sorted;
  });
};
