// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {HTMLMediaElement} videoTag <video> tag to render to.
 * @constructor
 */
remoting.MediaSourceRenderer = function(videoTag) {
  /** @type {HTMLMediaElement} */
  this.video_ = videoTag;

  /** @type {MediaSource} */
  this.mediaSource_ = null;

  /** @type {SourceBuffer} */
  this.sourceBuffer_ = null;

  /** @type {!Array.<ArrayBuffer>} Queue of pending buffers that haven't been
   * processed. A null element indicates that the SourceBuffer can be reset
   * because the following buffer contains a keyframe. */
  this.buffers_ = [];

  this.lastKeyFramePos_ = 0;
}

/**
 * @param {string} format Format of the stream.
 */
remoting.MediaSourceRenderer.prototype.reset = function(format) {
  // Reset the queue.
  this.buffers_ = [];

  // Create a new MediaSource instance.
  this.sourceBuffer_ = null;
  this.mediaSource_ = new MediaSource();
  this.mediaSource_.addEventListener('sourceopen',
                                     this.onSourceOpen_.bind(this, format));
  this.mediaSource_.addEventListener('sourceclose', function(e) {
    console.error("MediaSource closed unexpectedly.");
  });
  this.mediaSource_.addEventListener('sourceended', function(e) {
    console.error("MediaSource ended unexpectedly.");
  });

  // Start playback from new MediaSource.
  this.video_.src =
      /** @type {string} */(
          window.URL.createObjectURL(/** @type {!Blob} */(this.mediaSource_)));
  this.video_.play();
}

/**
 * @param {string} format
 * @private
 */
remoting.MediaSourceRenderer.prototype.onSourceOpen_ = function(format) {
  this.sourceBuffer_ =
      this.mediaSource_.addSourceBuffer(format);

  this.sourceBuffer_.addEventListener(
      'updateend', this.processPendingData_.bind(this));
  this.processPendingData_();
}

/**
 * @private
 */
remoting.MediaSourceRenderer.prototype.processPendingData_ = function() {
  if (this.sourceBuffer_) {
    while (this.buffers_.length > 0 && !this.sourceBuffer_.updating) {
      var buffer = /** @type {ArrayBuffer} */ this.buffers_.shift();
      if (buffer == null) {
        // Remove data from the SourceBuffer from the beginning to the previous
        // key frame. By default Chrome buffers up to 150MB of data. We never
        // need to seek the stream, so it doesn't make sense to keep any of that
        // data.
        if (this.sourceBuffer_.buffered.length > 0) {
          // TODO(sergeyu): Check currentTime to make sure that the current
          // playback position is not being removed. crbug.com/398290 .
          if (this.lastKeyFramePos_ > this.sourceBuffer_.buffered.start(0)) {
            this.sourceBuffer_.remove(this.sourceBuffer_.buffered.start(0),
                                      this.lastKeyFramePos_);
          }

          this.lastKeyFramePos_ = this.sourceBuffer_.buffered.end(
              this.sourceBuffer_.buffered.length - 1);
        }
      } else {
        // TODO(sergeyu): Figure out the way to determine when a frame is
        // rendered and use it to report performance statistics.
        this.sourceBuffer_.appendBuffer(buffer);
      }
    }
  }
}

/**
 * @param {ArrayBuffer} data
 * @param {boolean} keyframe
 */
remoting.MediaSourceRenderer.prototype.onIncomingData =
    function(data, keyframe) {
  if (keyframe) {
    // Queue SourceBuffer reset request.
    this.buffers_.push(null);
  }
  this.buffers_.push(data);
  this.processPendingData_();
}

