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
var CAPTURE_SIZE = {width: 576, height: 576};

/**
 * Interval between frames for camera capture (milliseconds).
 * @const
 */
var CAPTURE_INTERVAL_MS = 1000 / 10;

/**
 * Duration of camera capture (milliseconds).
 * @const
 */
var CAPTURE_DURATION_MS = 1000;

Polymer({
  is: 'cr-camera',

  behaviors: [CrPngBehavior],

  properties: {
    /** Strings provided by host */
    takePhotoLabel: String,
    captureVideoLabel: String,
    switchModeToCameraLabel: String,
    switchModeToVideoLabel: String,

    /** True if video mode is enabled. */
    videoModeEnabled: {
      type: Boolean,
      value: false,
    },

    /**
     * True if currently in video mode.
     * @private {boolean}
     */
    videomode: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * True when the camera is actually streaming video. May be false even when
     * the camera is present and shown, but still initializing.
     * @private {boolean}
     */
    cameraOnline_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {boolean} */
  cameraStartInProgress_: false,

  /** @private {boolean} */
  cameraCaptureInProgress_: false,

  /** @override */
  attached: function() {
    this.$.cameraVideo.addEventListener('canplay', function() {
      this.$.userImageStreamCrop.classList.add('preview');
      this.cameraOnline_ = true;
    }.bind(this));
    this.startCamera();
  },

  /** @override */
  detached: function() {
    this.stopCamera();
  },

  /** Only focuses the button if it's not disabled. */
  focusTakePhotoButton: function() {
    if (this.cameraOnline_)
      this.$.takePhoto.focus();
  },

  /**
   * Performs photo capture from the live camera stream. A 'photo-taken' event
   * will be fired as soon as captured photo is available, with the
   * 'photoDataURL' property containing the photo encoded as a data URL.
   */
  takePhoto: function() {
    if (!this.cameraOnline_ || this.cameraCaptureInProgress_)
      return;
    this.cameraCaptureInProgress_ = true;

    /** Pre-allocate all frames needed for capture. */
    var frames = [];
    if (this.videomode) {
      /** Reduce capture size when in video mode. */
      var captureSize = {
        width: CAPTURE_SIZE.width / 2,
        height: CAPTURE_SIZE.height / 2
      };
      var captureFrameCount = CAPTURE_DURATION_MS / CAPTURE_INTERVAL_MS;
      while (frames.length < captureFrameCount)
        frames.push(this.allocateFrame_(captureSize));
    } else {
      frames.push(this.allocateFrame_(CAPTURE_SIZE));
    }

    /** Start capturing frames at an interval. */
    var capturedFrames = [];
    this.$.userImageStreamCrop.classList.remove('preview');
    this.$.userImageStreamCrop.classList.add('capture');
    var interval = setInterval(() => {
      /** Stop capturing frames when all allocated frames have been consumed. */
      if (frames.length) {
        capturedFrames.push(
            this.captureFrame_(this.$.cameraVideo, frames.pop()));
      } else {
        clearInterval(interval);
        this.fire(
            'photo-taken',
            {photoDataUrl: this.convertFramesToPng_(capturedFrames)});
        this.$.userImageStreamCrop.classList.remove('capture');
        this.cameraCaptureInProgress_ = false;
      }
    }, CAPTURE_INTERVAL_MS);
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

    var videoConstraints = {
      width: {ideal: CAPTURE_SIZE.width},
      height: {ideal: CAPTURE_SIZE.height},
    };
    navigator.webkitGetUserMedia(
        {video: videoConstraints}, successCallback, errorCallback);
  },

  /** Stops the camera stream capture if it's currently active. */
  stopCamera: function() {
    this.$.userImageStreamCrop.classList.remove('preview');
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
   * Switch between photo and video mode.
   * @private
   */
  onTapSwitchMode_: function() {
    this.videomode = !this.videomode;
    this.fire('switch-mode', this.videomode);
  },

  /**
   * Allocates a canvas for capturing a single still frame at a specific size.
   * @param {{width: number, height: number}} size Frame size.
   * @return {!HTMLCanvasElement} The allocated canvas.
   * @private
   */
  allocateFrame_: function(size) {
    var canvas =
        /** @type {!HTMLCanvasElement} */ (document.createElement('canvas'));
    canvas.width = size.width;
    canvas.height = size.height;
    var ctx = /** @type {!CanvasRenderingContext2D} */ (
        canvas.getContext('2d', {alpha: false}));
    // Flip frame horizontally.
    ctx.translate(size.width, 0);
    ctx.scale(-1.0, 1.0);
    return canvas;
  },

  /**
   * Captures a single still frame from a <video> element, placing it at the
   * current drawing origin of a canvas context.
   * @param {!HTMLVideoElement} video Video element to capture from.
   * @param {!HTMLCanvasElement} canvas Canvas to save frame in.
   * @return {!HTMLCanvasElement} The canvas frame was saved in.
   * @private
   */
  captureFrame_: function(video, canvas) {
    var ctx =
        /** @type {!CanvasRenderingContext2D} */ (
            canvas.getContext('2d', {alpha: false}));
    var width = video.videoWidth;
    var height = video.videoHeight;
    if (width < canvas.width || height < canvas.height) {
      console.error(
          'Video capture size too small: ' + width + 'x' + height + '!');
    }
    var src = {};
    if (width / canvas.width > height / canvas.height) {
      // Full height, crop left/right.
      src.height = height;
      src.width = height * canvas.width / canvas.height;
    } else {
      // Full width, crop top/bottom.
      src.width = width;
      src.height = width * canvas.height / canvas.width;
    }
    src.x = (width - src.width) / 2;
    src.y = (height - src.height) / 2;
    ctx.drawImage(
        video, src.x, src.y, src.width, src.height, 0, 0, canvas.width,
        canvas.height);
    return canvas;
  },

  /**
   * Encode frames and convert to animated PNG image.
   * @param {!Array<!HTMLCanvasElement>} frames The frames to convert to image.
   * @return {!string} The data URL for image.
   * @private
   */
  convertFramesToPng_: function(frames) {
    /** Encode captured frames. */
    var encodedImages = frames.map(function(frame) {
      return frame.toDataURL('image/png');
    });

    /** No need for further processing if single frame. */
    if (encodedImages.length == 1)
      return encodedImages[0];

    /** Create forward/backward image sequence. */
    var forwardBackwardImageSequence =
        encodedImages.concat(encodedImages.slice(1, -1).reverse());

    /** Convert image sequence to animated PNG. */
    return CrPngBehavior.convertImageSequenceToPng(
        forwardBackwardImageSequence);
  },

  /**
   * Returns the label to use for take photo button.
   * @return {string}
   * @private
   */
  getTakePhotoLabel_: function(videomode, photoLabel, videoLabel) {
    return videomode ? videoLabel : photoLabel;
  },

  /**
   * Returns the label to use for switch mode button.
   * @return {string}
   * @private
   */
  getSwitchModeLabel_: function(videomode, cameraLabel, videoLabel) {
    return videomode ? cameraLabel : videoLabel;
  },
});

})();
