// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Inverval for updating media info (in ms).
 * @type {number}
 * @const
 */
var MEDIA_UPDATE_INTERVAL = 250;

/**
 * This class is the dummy class which has same interface as VideoElement. This
 * behaves like VideoElement, and is used for making Chromecast player
 * controlled instead of the true Video Element tag.
 *
 * @param {chrome.cast.media.MediaInfo} mediaInfo Data of the media to play.
 * @param {chrome.cast.Session} session Session to play a video on.
 * @constructor
 */
function CastVideoElement(mediaInfo, session) {
  this.mediaInfo_ = mediaInfo;
  this.castMedia_ = null;
  this.castSession_ = session;
  this.currentTime_ = null;
  this.src_ = '';
  this.volume_ = 100;
  this.currentMediaPlayerState_ = null;
  this.currentMediaCurrentTime_ = null;
  this.currentMediaDuration_ = null;
  this.pausing_ = false;

  this.onCastMediaUpdatedBound_ = this.onCastMediaUpdated_.bind(this);
}

CastVideoElement.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Prepares for unloading this objects.
   */
  dispose: function() {
    this.unloadMedia_();
  },

  /**
   * Returns a parent node. This must always be null.
   * @type {Element}
   */
  get parentNode() {
    return null;
  },

  /**
   * The total time of the video (in sec).
   * @type {?number}
   */
  get duration() {
    return this.currentMediaDuration_;
  },

  /**
   * The current timestamp of the video (in sec).
   * @type {?number}
   */
  get currentTime() {
    return this.castMedia_ ? this.castMedia_.getEstimatedTime() : null;
  },
  set currentTime(currentTime) {
    // TODO(yoshiki): Support seek.
  },

  /**
   * If this video is pauses or not.
   * @type {boolean}
   */
  get paused() {
    if (!this.castMedia_)
      return false;

    return this.pausing_ ||
           this.castMedia_.playerState === chrome.cast.media.PlayerState.PAUSED;
  },

  /**
   * If this video is ended or not.
   * @type {boolean}
   */
  get ended() {
    if (!this.castMedia_)
      return true;

   return this.castMedia_.idleReason === chrome.cast.media.IdleReason.FINISHED;
  },

  /**
   * If this video is seelable or not.
   * @type {boolean}
   */
  get seekable() {
    // TODO(yoshiki): Support seek.
    return false;
  },

  /**
   * Value of the volume
   * @type {number}
   */
  get volume() {
    return this.castSession_.receiver.volume.muted ?
               0 :
               this.castSession_.receiver.volume.level;
  },
  set volume(volume) {
    var VOLUME_EPS = 0.01;  // Threshold for ignoring a small change.

    // Ignores < 1% change.
    if (Math.abs(this.castSession_.receiver.volume.level - volume) < VOLUME_EPS)
      return;

    if (this.castSession_.receiver.volume.muted) {
      if (volume < VOLUME_EPS)
        return;

      // Unmute before setting volume.
      this.castSession_.setReceiverMuted(false,
          function() {},
          this.onCastCommandError_.wrap(this));

      this.castSession_.setReceiverVolumeLevel(volume,
          function() {},
          this.onCastCommandError_.wrap(this));
    } else {
      if (volume < VOLUME_EPS) {
        this.castSession_.setReceiverMuted(true,
            function() {},
            this.onCastCommandError_.wrap(this));
        return;
      }

      this.castSession_.setReceiverVolumeLevel(volume,
          function() {},
          this.onCastCommandError_.wrap(this));
    }
  },

  /**
   * Returns the source of the current video.
   * @type {?string}
   */
  get src() {
    return null;
  },
  set src(value) {
    // Do nothing.
  },

  /**
   * Plays the video.
   */
  play: function() {
    if (!this.castMedia_) {
      this.load(function() {
        this.castMedia_.play(null,
            function () {},
            this.onCastCommandError_.wrap(this));
      }.wrap(this));
      return;
    }

    this.castMedia_.play(null,
        function () {},
        this.onCastCommandError_.wrap(this));
  },

  /**
   * Pauses the video.
   */
  pause: function() {
    if (!this.castMedia_)
      return;

    this.pausing_ = true;
    this.castMedia_.pause(null,
        function () {
          this.pausing_ = false;
        }.wrap(this),
        function () {
          this.pausing_ = false;
          this.onCastCommandError_();
        }.wrap(this));
  },

  /**
   * Loads the video.
   */
  load: function(opt_callback) {
    var request = new chrome.cast.media.LoadRequest(this.mediaInfo_);
    this.castSession_.loadMedia(request,
        function(media) {
          this.onMediaDiscovered_(media);
          if (opt_callback)
            opt_callback();
        }.bind(this),
        this.onCastCommandError_.wrap(this));
  },

  /**
   * Unloads the video.
   * @private
   */
  unloadMedia_: function() {
    if (this.castMedia_) {
      this.castMedia_.stop(null,
          function () {},
          this.onCastCommandError_.wrap(this));

      this.castMedia_.removeUpdateListener(this.onCastMediaUpdatedBound_);
      this.castMedia_ = null;
    }
    clearInterval(this.updateTimerId_);
  },

  /**
   * This method is called periodically to update media information while the
   * media is loaded.
   * @private
   */
  onPeriodicalUpdateTimer_: function() {
    if (!this.castMedia_)
      return;

    if (this.castMedia_.playerState === chrome.cast.media.PlayerState.PLAYING)
      this.onCastMediaUpdated_(true);
  },

  /**
   * This method should be called when a media file is loaded.
   * @param {chrome.cast.Media} media Media object which was discovered.
   * @private
   */
  onMediaDiscovered_: function(media) {
    if (this.castMedia_ !== null) {
      this.unloadMedia_();
      console.info('New media is found and the old media is overridden.');
    }

    this.castMedia_ = media;
    this.onCastMediaUpdated_(true);
    media.addUpdateListener(this.onCastMediaUpdatedBound_);
    this.updateTimerId_ = setInterval(this.onPeriodicalUpdateTimer_.bind(this),
                                      MEDIA_UPDATE_INTERVAL);
  },

  /**
   * This method should be called when a media command to cast is failed.
   * @private
   */
  onCastCommandError_: function() {
    this.unloadMedia_();
    this.dispatchEvent(new Event('error'));
  },

  /**
   * This is called when any media data is updated and by the periodical timer
   * is fired.
   *
   * @param {boolean} alive Media availability. False if it's unavailable.
   * @private
   */
  onCastMediaUpdated_: function(alive) {
    if (!this.castMedia_)
      return;

    var media = this.castMedia_;
    if (this.currentMediaPlayerState_ !== media.playerState) {
      var oldPlayState = false;
      var oldState = this.currentMediaPlayerState_;
      if (oldState === chrome.cast.media.PlayerState.BUFFERING ||
          oldState === chrome.cast.media.PlayerState.PLAYING) {
        oldPlayState = true;
      }
      var newPlayState = false;
      var newState = media.playerState;
      if (newState === chrome.cast.media.PlayerState.BUFFERING ||
          newState === chrome.cast.media.PlayerState.PLAYING) {
        newPlayState = true;
      }
      if (!oldPlayState && newPlayState)
        this.dispatchEvent(new Event('play'));
      if (oldPlayState && !newPlayState)
        this.dispatchEvent(new Event('pause'));

      this.currentMediaPlayerState_ = newState;
    }
    if (this.currentMediaCurrentTime_ !== media.getEstimatedTime()) {
      this.currentMediaCurrentTime_ = media.getEstimatedTime();
      this.dispatchEvent(new Event('timeupdate'));
    }

    if (this.currentMediaDuration_ !== media.media.duration) {
      this.currentMediaDuration_ = media.media.duration;
      this.dispatchEvent(new Event('durationchange'));
    }

    // Media is being unloaded.
    if (!alive) {
      this.unloadMedia_();
      return;
    }
  },
};
