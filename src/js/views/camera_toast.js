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
 * Creates a controller for the toast of Camera view.
 * @constructor
 */
camera.views.Toast = function() {
  /**
   * Timer for hiding the toast message after some delay.
   * @type {?number}
   * @private
   */
  this.toastHideTimer_ = null;

  /**
   * Toast transition wrapper. Shows or hides the toast with the passed message.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toastEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        var toastElement = document.querySelector('#toast');
        var toastMessageElement = document.querySelector('#toast-message');
        // Hide the message if visible.
        if (!args.visible && toastElement.classList.contains('visible')) {
          toastElement.classList.remove('visible');
          camera.util.waitForTransitionCompletion(
              toastElement, 500, callback);
        } else if (args.visible) {
          // If showing requested, then show.
          toastMessageElement.textContent = args.message;
          toastElement.classList.add('visible');
          camera.util.waitForTransitionCompletion(
             toastElement, 500, callback);
        } else {
          callback();
        }
      }.bind(this));

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Shows a message in the toast.
 * @param {string} message Message to be shown.
 * @private
 */
camera.views.Toast.prototype.showMessage = function(message) {
  var cancelHideTimer = function() {
    if (this.toastHideTimer_) {
      clearTimeout(this.toastHideTimer_);
      this.toastHideTimer_ = null;
    }
  }.bind(this);
  // If running, then reinvoke recursively after closing the toast message.
  if (this.toastEffect_.animating || this.toastHideTimer_) {
    cancelHideTimer();
    this.toastEffect_.invoke({
      visible: false
    }, this.showMessage.bind(this, message));
    return;
  }
  // Cancel any pending hide timers.
  cancelHideTimer();
  // Start the hide timer.
  this.toastHideTimer_ = setTimeout(function() {
    this.toastEffect_.invoke({
      visible: false
    }, function() {});
    this.toastHideTimer_ = null;
  }.bind(this), 2000);
  // Show the toast message.
  this.toastEffect_.invoke({
    visible: true,
    message: message
  }, function() {});
};
