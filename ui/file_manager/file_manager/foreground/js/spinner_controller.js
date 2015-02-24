// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for spinner.
 * @param {!HTMLElement} element
 * @param {!DirectoryModel} directoryModel
 * @constructor
 * @extends {cr.EventTarget}
 */
function SpinnerController(element, directoryModel) {
  /**
   * The container element of the file list.
   * @type {!HTMLElement}
   * @const
   * @private
   */
  this.element_ = element;

  /**
   * Directory model.
   * @type {!DirectoryModel}
   * @const
   * @private
   */
  this.directoryModel_ = directoryModel;

  /**
   * @type {number}
   * @private
   */
  this.timeoutId_ = 0;
}

SpinnerController.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Shows the spinner.
 */
SpinnerController.prototype.show = function() {
  if (!this.directoryModel_.isScanning())
    return;
  this.element_.hidden = false;
  clearTimeout(this.timeoutId_);
  this.timeoutId_ = 0;
  var spinnerShownEvent = new Event('spinner-shown');
  this.dispatchEvent(spinnerShownEvent);
};

/**
 * Hides the spinner.
 */
SpinnerController.prototype.hide = function() {
  if (this.directoryModel_.isScanning() &&
      this.directoryModel_.getFileList().length == 0) {
    return;
  }
  this.element_.hidden = true;
  clearTimeout(this.timeoutId_);
  this.timeoutId_ = 0;
};

/**
 * Shows the spinner after 500ms.
 */
SpinnerController.prototype.showLater = function() {
  if (!this.element_.hidden)
    return;
  clearTimeout(this.timeoutId_);
  this.timeoutId_ = setTimeout(this.show.bind(this), 500);
};
