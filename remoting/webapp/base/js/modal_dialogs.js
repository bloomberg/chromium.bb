// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * A helper class for implementing dialogs with an input field using
 * remoting.setMode().
 *
 * @param {remoting.AppMode} mode
 * @param {HTMLElement} formElement
 * @param {HTMLElement} inputField
 * @param {HTMLElement} cancelButton
 *
 * @constructor
 */
remoting.InputDialog = function(mode, formElement, inputField, cancelButton) {
  /** @private */
  this.appMode_ = mode;
  /** @private */
  this.formElement_ = formElement;
  /** @private */
  this.cancelButton_ = cancelButton;
  /** @private */
  this.inputField_ = inputField;
  /** @private {base.Deferred} */
  this.deferred_ = null;
  /** @private {base.Disposables} */
  this.eventHooks_ = null;
};

/**
 * @return {Promise<string>}  Promise that resolves with the value of the
 *    inputField or rejects with |remoting.Error.CANCELLED| if the user clicks
 *    on the cancel button.
 */
remoting.InputDialog.prototype.show = function() {
  var onCancel = this.createFormEventHandler_(this.onCancel_.bind(this));
  var onOk = this.createFormEventHandler_(this.onSubmit_.bind(this));

  this.eventHooks_ = new base.Disposables(
      new base.DomEventHook(this.formElement_, 'submit', onOk, false),
      new base.DomEventHook(this.cancelButton_, 'click', onCancel, false));
  base.debug.assert(this.deferred_ === null);
  this.deferred_ = new base.Deferred();
  remoting.setMode(this.appMode_);
  return this.deferred_.promise();
};

/** @return {HTMLElement} */
remoting.InputDialog.prototype.inputField = function() {
  return this.inputField_;
}

/** @private */
remoting.InputDialog.prototype.onSubmit_ = function() {
  this.deferred_.resolve(this.inputField_.value);
}

/** @private */
remoting.InputDialog.prototype.onCancel_ = function() {
  this.deferred_.reject(new remoting.Error(remoting.Error.Tag.CANCELLED));
}

/**
 * @param {function():void} handler
 * @return {Function}
 * @private
 */
remoting.InputDialog.prototype.createFormEventHandler_ = function(handler) {
  var that = this;
  return function (/** Event */ e) {
    // Prevents form submission from reloading the v1 app.
    e.preventDefault();

    // Set the focus away from the password field. This has to be done
    // before the password field gets hidden, to work around a Blink
    // clipboard-handling bug - http://crbug.com/281523.
    that.cancelButton_.focus();
    handler();
    base.dispose(that.eventHooks_);
    that.eventHooks_ = null;
    that.deferred_ = null;
  };
};

/**
 * A helper class for implementing MessageDialog with a primary and
 * and secondary button using remoting.setMode().
 *
 * @param {remoting.AppMode} mode
 * @param {HTMLElement} primaryButton
 * @param {HTMLElement=} opt_secondaryButton
 * @constructor
 */
remoting.MessageDialog = function(mode, primaryButton, opt_secondaryButton) {
  /** @private @const */
  this.mode_ = mode;
  /** @private @const */
  this.primaryButton_ = primaryButton;
  /** @private @const */
  this.secondaryButton_ = opt_secondaryButton;
  /** @private {base.Deferred} */
  this.deferred_ = null;
  /** @private {base.Disposables} */
  this.eventHooks_ = null;
};

/**
 * @return {Promise<remoting.MessageDialog.Result>}  Promise that resolves with
 * the button clicked.
 */
remoting.MessageDialog.prototype.show = function() {
  this.eventHooks_ = new base.Disposables(new base.DomEventHook(
      this.primaryButton_, 'click',
      this.onClicked_.bind(this, remoting.MessageDialog.Result.PRIMARY),
      false));

  if (this.secondaryButton_) {
    this.eventHooks_.add(new base.DomEventHook(
        this.secondaryButton_, 'click',
        this.onClicked_.bind(this, remoting.MessageDialog.Result.SECONDARY),
        false));
  }

  base.debug.assert(this.deferred_ === null);
  this.deferred_ = new base.Deferred();
  remoting.setMode(this.mode_);
  return this.deferred_.promise();
};

/**
 * @param {remoting.MessageDialog.Result} result
 * @return {Function}
 * @private
 */
remoting.MessageDialog.prototype.onClicked_ = function(result) {
  this.deferred_.resolve(result);
  base.dispose(this.eventHooks_);
  this.eventHooks_ = null;
  this.deferred_ = null;
};

})();

/**
 * Define the enum at the end of file as JSCompile doesn't understand enums that
 * are defined within an IIFE (Immediately Invoked Function Expression).
 * @enum {number}
 */
remoting.MessageDialog.Result = {
  PRIMARY: 0,
  SECONDARY: 1
};