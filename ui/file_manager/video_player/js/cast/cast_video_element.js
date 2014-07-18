// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * This class is the dummy class which has same interface as VideoElement. This
 * behaves like VideoElement, and is used for making Chromecast player
 * controlled instead of the true Video Element tag.
 *
 * @constructor
 */
function CastVideoElement() {
  this.duration_ = null;
  this.currentTime_ = null;
  this.src_ = '';
  this.volume_ = 100;
}

CastVideoElement.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Returns a parent node. This must always be null.
   * @return {Element}
   */
  get parentNode() {
    return null;
  },

  /**
   * The total time of the video.
   * @type {number}
   */
  get duration() {
    return this.duration_;
  },

  /**
   * The current timestamp of the video.
   * @type {number}
   */
  get currentTime() {
    return this.currentTime_;
  },
  set currentTime(currentTime) {
    this.currentTime_ = currentTime;
  },

  /**
   * If this video is pauses or not.
   * @type {boolean}
   */
  get paused() {
    return false;
  },

  /**
   * If this video is ended or not.
   * @type {boolean}
   */
  get ended() {
    return false;
  },

  /**
   * If this video is seelable or not.
   * @type {boolean}
   */
  get seekable() {
    return false;
  },

  /**
   * Value of the volume
   * @type {number}
   */
  get volume() {
    return this.volume_;
  },
  set volume(volume) {
    this.volume_ = volume;
  },

  /**
   * Returns the source of the current video.
   * @return {string}
   */
  get src() {
    return this.src_;
  },

  /**
   * Plays the video.
   */
  play: function() {
    // TODO(yoshiki): Implement this.
  },

  /**
   * Pauses the video.
   */
  pause: function() {
    // TODO(yoshiki): Implement this.
  },

  /**
   * Loads the video.
   */
  load: function() {
    // TODO(yoshiki): Implement this.
  },
};
