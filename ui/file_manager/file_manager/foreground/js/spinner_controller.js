// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for spinner.
 * @param {!HTMLElement} element
 * @constructor
 * @extends {cr.EventTarget}
 */
function SpinnerController(element) {
  /**
   * The container element of the file list.
   * @type {!HTMLElement}
   * @const
   * @private
   */
  this.element_ = element;

  /**
   * @type {number}
   * @private
   */
  this.timeoutId_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.blinkHideTimeoutId_ = 0;
}

SpinnerController.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Blinks the spinner for a short period of time.
 */
SpinnerController.prototype.blink = function() {
  this.element_.hidden = false;
  clearTimeout(this.blinkHideTimeoutId_);
  this.blinkHideTimeoutId_ = setTimeout(function() {
    this.element_.hidden = true;
    this.blinkHideTimeoutId_ = 0;
  }.bind(this), 1000);
};

/**
 * Shows the spinner until hide is called.
 */
SpinnerController.prototype.show = function() {
  this.element_.hidden = false;
  clearTimeout(this.blinkHideTimeoutId_);
  this.blinkHideTimeoutId_ = 0;
  clearTimeout(this.timeoutId_);
  this.timeoutId_ = 0;
  var spinnerShownEvent = new Event('spinner-shown');
  this.dispatchEvent(spinnerShownEvent);
};

/**
 * Hides the spinner.
 */
SpinnerController.prototype.hide = function() {
  // If the current spinner is a blink, then it will hide by itself shortly.
  if (this.blinkHideTimeoutId_)
    return;

  this.element_.hidden = true;
  clearTimeout(this.timeoutId_);
  this.timeoutId_ = 0;
};

/**
 * Shows the spinner after 500ms (unless it's already visible).
 */
SpinnerController.prototype.showLater = function() {
  if (!this.element_.hidden) {
    // If there is an ongoing blink, then keep it visible until hide() is
    // called.
    clearTimeout(this.blinkHideTimeoutId_);
    this.blinkHideTimeoutId_ = 0;
    return;
  }

  clearTimeout(this.timeoutId_);
  this.timeoutId_ = setTimeout(this.show.bind(this), 500);
};
