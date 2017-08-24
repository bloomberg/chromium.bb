// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-camera' is a Polymer element used to take a picture from the
 * user webcam to use as a Chrome OS profile picture.
 */
(function() {

/**
 * Dimensions for camera capture.
 * @const
 */
var CAPTURE_SIZE = {height: 576, width: 576};

Polymer({
  is: 'cr-camera',

  properties: {
    /** Strings provided by host */
    flipPhotoLabel: String,
    takePhotoLabel: String,

    /**
     * True when the camera is actually streaming video. May be false even when
     * the camera is present and shown, but still initializing.
     * @private {boolean}
     */
    cameraOnline_: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the photo is currently marked flipped.
     * @private {boolean}
     */
    isFlipped_: {
      type: Boolean,
      value: true,
    },
  },

  /** @override */
  attached: function() {
    this.$.userImageStreamCrop.classList.toggle('flip-x', this.isFlipped_);
    this.$.cameraVideo.addEventListener('canplay', function() {
      this.cameraOnline_ = true;
    }.bind(this));
    this.startCamera();
  },

  /** @override */
  detached: function() {
    this.stopCamera();
  },

  /**
   * Performs photo capture from the live camera stream. A 'photo-taken' event
   * will be fired as soon as captured photo is available, with the
   * 'photoDataURL' property containing the photo encoded as a data URL.
   */
  takePhoto: function() {
    if (!this.cameraOnline_)
      return;
    var canvas =
        /** @type {HTMLCanvasElement} */ (document.createElement('canvas'));
    canvas.width = CAPTURE_SIZE.width;
    canvas.height = CAPTURE_SIZE.height;
    this.captureFrame_(
        this.$.cameraVideo,
        /** @type {!CanvasRenderingContext2D} */ (canvas.getContext('2d')));

    var photoDataUrl = this.isFlipped_ ? this.flipFrame_(canvas) :
                                         canvas.toDataURL('image/png');
    this.fire('photo-taken', {photoDataUrl: photoDataUrl});
  },

  /** Tries to start the camera stream capture. */
  startCamera: function() {
    this.stopCamera();
    this.cameraStartInProgress_ = true;

    var successCallback = function(stream) {
      if (this.cameraStartInProgress_) {
        this.$.cameraVideo.src = URL.createObjectURL(stream);
        this.cameraStream_ = stream;
      } else {
        this.stopVideoTracks_(stream);
      }
      this.cameraStartInProgress_ = false;
    }.bind(this);

    var errorCallback = function() {
      this.cameraOnline_ = false;
      this.cameraStartInProgress_ = false;
    }.bind(this);

    navigator.webkitGetUserMedia({video: true}, successCallback, errorCallback);
  },

  /** Stops the camera stream capture if it's currently active. */
  stopCamera: function() {
    this.cameraOnline_ = false;
    this.$.cameraVideo.src = '';
    if (this.cameraStream_)
      this.stopVideoTracks_(this.cameraStream_);
    // Cancel any pending getUserMedia() checks.
    this.cameraStartInProgress_ = false;
  },

  /**
   * Stops all video tracks associated with a MediaStream object.
   * @param {!MediaStream} stream
   * @private
   */
  stopVideoTracks_: function(stream) {
    var tracks = stream.getVideoTracks();
    for (var i = 0; i < tracks.length; i++)
      tracks[i].stop();
  },

  /**
   * Flip the live camera stream.
   * @private
   */
  onTapFlipPhoto_: function() {
    this.isFlipped_ = !this.isFlipped_;
    this.$.userImageStreamCrop.classList.toggle('flip-x', this.isFlipped_);
    this.fire('photo-flipped', this.isFlipped_);
  },

  /**
   * Captures a single still frame from a <video> element, placing it at the
   * current drawing origin of a canvas context.
   * @param {!HTMLVideoElement} video Video element to capture from.
   * @param {!CanvasRenderingContext2D} ctx Canvas context to draw onto.
   * @private
   */
  captureFrame_: function(video, ctx) {
    var width = video.videoWidth;
    var height = video.videoHeight;
    if (width < CAPTURE_SIZE.width || height < CAPTURE_SIZE.height) {
      console.error(
          'Video capture size too small: ' + width + 'x' + height + '!');
    }
    var src = {};
    if (width / CAPTURE_SIZE.width > height / CAPTURE_SIZE.height) {
      // Full height, crop left/right.
      src.height = height;
      src.width = height * CAPTURE_SIZE.width / CAPTURE_SIZE.height;
    } else {
      // Full width, crop top/bottom.
      src.width = width;
      src.height = width * CAPTURE_SIZE.height / CAPTURE_SIZE.width;
    }
    src.x = (width - src.width) / 2;
    src.y = (height - src.height) / 2;
    ctx.drawImage(
        video, src.x, src.y, src.width, src.height, 0, 0, CAPTURE_SIZE.width,
        CAPTURE_SIZE.height);
  },

  /**
   * Flips frame horizontally.
   * @param {!(HTMLImageElement|HTMLCanvasElement|HTMLVideoElement)} source
   *     Frame to flip.
   * @return {string} Flipped frame as data URL.
   */
  flipFrame_: function(source) {
    var canvas = document.createElement('canvas');
    canvas.width = CAPTURE_SIZE.width;
    canvas.height = CAPTURE_SIZE.height;
    var ctx = canvas.getContext('2d');
    ctx.translate(CAPTURE_SIZE.width, 0);
    ctx.scale(-1.0, 1.0);
    ctx.drawImage(source, 0, 0);
    return canvas.toDataURL('image/png');
  },
});

})();
