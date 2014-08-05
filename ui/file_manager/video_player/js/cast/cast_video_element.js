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
 * The namespace for communication between the cast and the player.
 * @type {string}
 * @const
 */
var CAST_MESSAGE_NAMESPACE = 'urn:x-cast:com.google.chromeos.videoplayer';

/**
 * This class is the dummy class which has same interface as VideoElement. This
 * behaves like VideoElement, and is used for making Chromecast player
 * controlled instead of the true Video Element tag.
 *
 * @param {MediaManager} media Media manager with the media to play.
 * @param {chrome.cast.Session} session Session to play a video on.
 * @constructor
 */
function CastVideoElement(media, session) {
  this.mediaManager_ = media;
  this.mediaInfo_ = null;

  this.castMedia_ = null;
  this.castSession_ = session;
  this.currentTime_ = null;
  this.src_ = '';
  this.volume_ = 100;
  this.currentMediaPlayerState_ = null;
  this.currentMediaCurrentTime_ = null;
  this.currentMediaDuration_ = null;
  this.playInProgress_ = false;
  this.pauseInProgress_ = false;

  this.onMessageBound_ = this.onMessage_.bind(this);
  this.onCastMediaUpdatedBound_ = this.onCastMediaUpdated_.bind(this);
  this.castSession_.addMessageListener(
      CAST_MESSAGE_NAMESPACE, this.onMessageBound_);
}

CastVideoElement.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Prepares for unloading this objects.
   */
  dispose: function() {
    this.unloadMedia_();
    this.castSession_.removeMessageListener(
        CAST_MESSAGE_NAMESPACE, this.onMessageBound_);
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
    if (this.castMedia_) {
      if (this.castMedia_.idleReason === chrome.cast.media.IdleReason.FINISHED)
        return this.currentMediaDuration_;  // Returns the duration.
      else
        return this.castMedia_.getEstimatedTime();
    } else {
      return null;
    }
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

    return !this.playInProgress_ &&
        (this.pauseInProgress_ ||
         this.castMedia_.playerState === chrome.cast.media.PlayerState.PAUSED);
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
    var play = function() {
      this.castMedia_.play(null,
          function() {
            this.playInProgress_ = false;
          }.wrap(this),
          function(error) {
            this.playInProgress_ = false;
            this.onCastCommandError_(error);
          }.wrap(this));
    }.wrap(this);

    this.playInProgress_ = true;

    if (!this.castMedia_)
      this.load(play);
    else
      play();
  },

  /**
   * Pauses the video.
   */
  pause: function() {
    if (!this.castMedia_)
      return;

    this.pauseInProgress_ = true;
    this.castMedia_.pause(null,
        function() {
          this.pauseInProgress_ = false;
        }.wrap(this),
        function(error) {
          this.pauseInProgress_ = false;
          this.onCastCommandError_(error);
        }.wrap(this));
  },

  /**
   * Loads the video.
   */
  load: function(opt_callback) {
    var sendTokenPromise = this.mediaManager_.getToken().then(function(token) {
      this.token_ = token;
      this.sendMessage_({message: 'push-token', token: token});
    }.bind(this));

    Promise.all([
      sendTokenPromise,
      this.mediaManager_.getUrl(),
      this.mediaManager_.getMime(),
      this.mediaManager_.getThumbnail()]).
        then(function(results) {
          var url = results[1];
          var mime = results[2];
          var thumbnailUrl = results[3];

          this.mediaInfo_ = new chrome.cast.media.MediaInfo(url);
          this.mediaInfo_.contentType = mime;
          this.mediaInfo_.customData = {
            tokenRequired: true,
            thumbnailUrl: thumbnailUrl,
          };

          var request = new chrome.cast.media.LoadRequest(this.mediaInfo_);
          return new Promise(
              this.castSession_.loadMedia.bind(this.castSession_, request)).
              then(function(media) {
                this.onMediaDiscovered_(media);
                if (opt_callback)
                  opt_callback();
              }.bind(this));
        }.bind(this)).catch(function(error) {
          this.unloadMedia_();
          this.dispatchEvent(new Event('error'));
          console.error('Cast failed.', error.stack || error);
        }.bind(this));
  },

  /**
   * Unloads the video.
   * @private
   */
  unloadMedia_: function() {
    if (this.castMedia_) {
      this.castMedia_.stop(null,
          function() {},
          function(error) {
            // Ignores session error, since session may already be closed.
            if (error.code !== chrome.cast.ErrorCode.SESSION_ERROR)
              this.onCastCommandError_(error);
          }.wrap(this));

      this.castMedia_.removeUpdateListener(this.onCastMediaUpdatedBound_);
      this.castMedia_ = null;
    }
    clearInterval(this.updateTimerId_);
  },

  /**
   * Sends the message to cast.
   * @param {Object} message Message to be sent (Must be JSON-able object).
   * @private
   */
  sendMessage_: function(message) {
    this.castSession_.sendMessage(CAST_MESSAGE_NAMESPACE, message);
  },

  /**
   * Invoked when receiving a message from the cast.
   * @param {string} namespace Namespace of the message.
   * @param {string} messageAsJson Content of message as json format.
   * @private
   */
  onMessage_: function(namespace, messageAsJson) {
    if (namespace !== CAST_MESSAGE_NAMESPACE || !messageAsJson)
      return;

    var message = JSON.parse(messageAsJson);
    if (message['message'] === 'request-token') {
      if (message['previousToken'] === this.token_) {
          this.mediaManager_.getToken().then(function(token) {
            this.sendMessage_({message: 'push-token', token: token});
            // TODO(yoshiki): Revokes the previous token.
          }.bind(this)).catch(function(error) {
            // Send an empty token as an error.
            this.sendMessage_({message: 'push-token', token: ''});
            // TODO(yoshiki): Revokes the previous token.
            console.error(error.stack || error);
          });
      } else {
        console.error(
            'New token is requested, but the previous token mismatches.');
      }
    }
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
    // Notify that the metadata of the video is ready.
    this.dispatchEvent(new Event('loadedmetadata'));

    media.addUpdateListener(this.onCastMediaUpdatedBound_);
    this.updateTimerId_ = setInterval(this.onPeriodicalUpdateTimer_.bind(this),
                                      MEDIA_UPDATE_INTERVAL);
  },

  /**
   * This method should be called when a media command to cast is failed.
   * @param {Object} error Object representing the error.
   * @private
   */
  onCastCommandError_: function(error) {
    this.unloadMedia_();
    this.dispatchEvent(new Event('error'));
    console.error('Error on sending command to cast.', error.stack || error);
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
