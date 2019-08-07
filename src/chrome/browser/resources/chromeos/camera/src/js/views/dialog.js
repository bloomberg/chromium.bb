// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Creates the Dialog view controller.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.Dialog = function() {
  cca.views.View.call(this, '#dialog', true);

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
   * @type {HTMLElement}
   * @private
   */
  this.messageElement_ = document.querySelector('#dialog-msg');

  // End of properties, seal the object.
  Object.seal(this);

  this.positiveButton_.addEventListener('click', () => this.leave(true));
  this.negativeButton_.addEventListener('click', () => this.leave());
};

cca.views.Dialog.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * @param {string} message Message of the dialog.
 * @param {boolean} cancellable Whether the dialog is cancellable.
 * @override
 */
cca.views.Dialog.prototype.entering = function(message, cancellable) {
  this.messageElement_.textContent = message;
  this.negativeButton_.hidden = !cancellable;
};

/**
 * @override
 */
cca.views.Dialog.prototype.focus = function() {
  this.positiveButton_.focus();
};
