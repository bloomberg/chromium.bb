// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates a view controller.
 * @param {camera.View.Context} context Context of the app.
 * @constructor
 */
camera.View = function(context) {
  /**
   * @type {boolean}
   * @private
   */
  this.active_ = false;

  /**
   * @type {camera.View.Context}
   * @private
   */
  this.context_ = context;
};

camera.View.prototype = {
  get active() {
    return this.active_;
  },
  get context() {
    return this.context_;
  }
};

/**
 * Processes key pressed events.
 * @param {Event} event Key event.
 */
camera.View.prototype.onKeyPressed = function(event) {
};

/**
 * Processes resizing events.
 */
camera.View.prototype.onResize = function() {
};

/**
 * Handles enters the view.
 */
camera.View.prototype.onEnter = function() {
};

/**
 * Handles leaving the view.
 */
camera.View.prototype.onLeave = function() {
};

/**
 * Initializes the view. Call the callback on completion.
 * @param {function()} callback Completion callback.
 */
camera.View.prototype.initialize = function(callback) {
  callback();
};

/**
 * Enters the view.
 */
camera.View.prototype.enter = function() {
  this.active_ = true;
  this.onEnter();
};

/**
 * Leaves the view.
 */
camera.View.prototype.leave = function() {
  this.active_ = false;
  this.onLeave();
};

/**
 * Creates a shared context object for views.
 * @constructor
 */
camera.View.Context = function() {
};

