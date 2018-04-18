// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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
 * Creates the Dialog view controller.
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @extends {camera.View}
 * @constructor
 */
camera.views.Dialog = function(context, router) {
  camera.View.call(
      this, context, router, document.querySelector('#dialog'), 'dialog');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.positiveButton_ = document.querySelector('#dialog-positive-button');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.negativeButton_ = document.querySelector('#dialog-negative-button');

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.closeButton_ = document.querySelector('#dialog-close-button');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.messageElement_ = document.querySelector('#dialog-msg');

  // End of properties, seal the object.
  Object.seal(this);

  // Register handler on the buttons.
  this.positiveButton_.addEventListener(
      'click', this.onPositiveButtonClicked_.bind(this));
  this.negativeButton_.addEventListener(
      'click', this.onNegativeButtonClicked_.bind(this));
  this.closeButton_.addEventListener(
      'click', this.onCloseButtonClicked_.bind(this));
};

/**
 * Type of the dialog.
 * @enum {number}
 */
camera.views.Dialog.Type = Object.freeze({
  CONFIRMATION: 0,
  ALERT: 1
});

camera.views.Dialog.prototype = {
  __proto__: camera.View.prototype
};

/**
 * Enters the view. Assumes, that the corresponding arguments are provided
 * according to each type:
 * CONFIRMATION type with 'message' argument,
 * ALERT type with 'message' argument,
 * @param {Object=} opt_arguments Arguments for the dialog.
 * @override
 */
camera.views.Dialog.prototype.onEnter = function(opt_arguments) {
  // Update the element's text content and hide it if there is no text content.
  var updateElement = function(element, text) {
    if (text) {
      element.textContent = text.content ? text.content :
          chrome.i18n.getMessage(text.name);
      element.hidden = false;
    } else {
      element.hidden = true;
    }
  };

  var positiveText = null;
  var negativeText = null;
  var messageText = null;
  switch (opt_arguments.type) {
    case camera.views.Dialog.Type.CONFIRMATION:
      positiveText = { name: 'dialogYesButton' };
      negativeText = { name: 'dialogNoButton' };
      messageText = { content: opt_arguments.message };
      break;
    case camera.views.Dialog.Type.ALERT:
      positiveText = { name: 'dialogOKButton' };
      messageText = { content: opt_arguments.message };
      break;
  }

  updateElement(this.positiveButton_, positiveText);
  updateElement(this.negativeButton_, negativeText);
  updateElement(this.messageElement_, messageText);

  // TODO(yuli): Hide the close button if showing the spinner or progress bar.
  this.closeButton_.hidden = false;
};

/**
 * @override
 */
camera.views.Dialog.prototype.onActivate = function() {
  if (!this.positiveButton_.hidden)
    this.positiveButton_.focus();
};

/**
 * Handles clicking on the positive button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Dialog.prototype.onPositiveButtonClicked_ = function(event) {
  this.router.back({isPositive: true});
};

/**
 * Handles clicking on the negative button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Dialog.prototype.onNegativeButtonClicked_ = function(event) {
  this.closeDialog_();
};

/**
 * Handles clicking on the close button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Dialog.prototype.onCloseButtonClicked_ = function(event) {
  this.closeDialog_();
};

/**
 * Dismisses the dialog without returning a positive result.
 * @private
 */
camera.views.Dialog.prototype.closeDialog_ = function() {
  this.router.back({isPositive: false});
};

/**
 * @override
 */
camera.views.Dialog.prototype.onKeyPressed = function(event) {
  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Enter':
      if (document.activeElement != this.positiveButton_)
        break;
      this.router.back({isPositive: true});
      event.preventDefault();
      break;
    case 'Escape':
      // Don't dismiss the dialog if there is no close-button.
      if (this.closeButton_.hidden)
        break;
      this.closeDialog_();
      event.preventDefault();
      break;
  }
};

