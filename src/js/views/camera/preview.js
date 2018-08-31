// Copyright 2018 The Chromium OS Authors. All rights reserved.
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
 * Creates a controller for the video preview of Camera view.
 * @constructor
 */
camera.views.camera.Preview = function() {
  /**
   * Video element to capture the stream.
   * @type {Video}
   * @private
   */
  this.video_ = document.querySelector('#preview-video');

  // End of properties, seal the object.
  Object.seal(this);

  this.video_.cleanup = () => {};
};

camera.views.camera.Preview.prototype = {
  get aspectRatio() {
    return this.video_.videoHeight ?
        (this.video_.videoWidth / this.video_.videoHeight) : 0;
  },
  get letterboxSize() {
    return [window.innerWidth - this.video_.width,
        window.innerHeight - this.video_.height];
  }
};

/**
 * @override
 */
camera.views.camera.Preview.prototype.toString = function() {
  return this.video_.videoHeight ?
      (this.video_.videoWidth + ' x ' + this.video_.videoHeight) : '';
};

/**
 * Sets video source.
 * @param {MediaStream} stream Stream to be the source.
 * @param {function()} onIntrinsicSize Callback for the intrinsic size of
 *     preview video is first fetched or changed by orientation changes.
 * @return {!Promise<MediaStream>} Promise for the stream set.
 */
camera.views.camera.Preview.prototype.setSource = function(
    stream, onIntrinsicSize) {
  return new Promise(resolve => {
    var video = document.createElement('video');
    video.id = 'preview-video';

    var onLoadedMetadata = () => {
      video.removeEventListener('loadedmetadata', onLoadedMetadata);
      video.play();
      this.video_.parentElement.replaceChild(video, this.video_);
      this.video_.cleanup();
      this.video_ = video;
      resolve(stream);
    };
    var onResize = () => {
      if (video.videoWidth && video.videoHeight) {
        onIntrinsicSize();
      }
    };
    video.addEventListener('loadedmetadata', onLoadedMetadata);
    video.addEventListener('resize', onResize);
    video.cleanup = () => {
      video.removeEventListener('resize', onResize);
      video.removeAttribute('srcObject');
      video.load();
    };
    video.muted = true;  // Mute to avoid echo from the captured audio.
    video.srcObject = stream;
  });
};

/**
 * Pauses the video.
 */
camera.views.camera.Preview.prototype.pause = function() {
  this.video_.pause();
};

/**
 * Layouts the video element's size for displaying in the window.
 */
camera.views.camera.Preview.prototype.layoutElementSize = function() {
  // Make video content keeps its aspect ratio inside the window's inner-bounds;
  // it may fill up the window or be letterboxed when fullscreen/maximized.
  // Don't use app-window.innerBounds' width/height properties during resizing
  // as they are not updated immediately.
  if (this.video_.videoHeight) {
    var f = camera.util.isWindowFullSize() ? Math.min : Math.max;
    var scale = f(window.innerHeight / this.video_.videoHeight,
        window.innerWidth / this.video_.videoWidth);
    this.video_.width = scale * this.video_.videoWidth;
    this.video_.height = scale * this.video_.videoHeight;
  }
}

/**
 * Creates an image blob of the current frame.
 * @return {!Promise<Blob>} Promise for the result.
 */
camera.views.camera.Preview.prototype.toImage = function() {
  var canvas = document.createElement('canvas');
  var context = canvas.getContext('2d');
  canvas.width = this.video_.videoWidth;
  canvas.height = this.video_.videoHeight;
  context.drawImage(this.video_, 0, 0);
  return new Promise((resolve, reject) => {
    canvas.toBlob(blob => {
      if (blob) {
        resolve(blob);
      } else {
        reject('Photo blob error.');
      }
    }, 'image/jpeg');
  });
};
