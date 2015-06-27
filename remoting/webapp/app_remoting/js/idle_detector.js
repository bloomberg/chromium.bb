// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class for detecting when the application is idle. Note that chrome.idle is
 * not suitable for this purpose because it detects when the computer is idle,
 * and we'd like to close the application and free up VM resources even if the
 * user has been using another application for a long time.
 *
 * There are two idle timeouts. The first controls the visibility of the idle
 * timeout warning dialog and is reset on mouse input; when it expires, the
 * idle warning dialog is displayed. The second controls the length of time
 * for which the idle warning dialog is displayed; when it expires, the ctor
 * callback is invoked, which it is assumed will exit the application--no
 * further idle detection is done.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {HTMLElement} rootElement The idle warning dialog.
 * @param {remoting.WindowShape} windowShape
 * @param {string} applicationName
 * @param {function():void} callback Called when the idle warning dialog has
 *     timed out or the user has explicitly indicated that they are no longer
 *     using the session.
 * @constructor
 * @implements {base.Disposable}
 */
remoting.IdleDetector = function(rootElement, windowShape, applicationName,
                                 callback) {
  /** @private */
  this.callback_ = callback;

  /**
   * @private {base.OneShotTimer}
   */
  this.timer_ = null;

  /** @private {?function():void} */
  this.resetTimeoutRef_ = null;

  var message = rootElement.querySelector('.idle-warning-message');
  l10n.localizeElement(message, applicationName);

  /** @private */
  this.dialog_ = new remoting.Html5ModalDialog({
    dialog: /** @type {HTMLDialogElement} */ (rootElement),
    primaryButton: rootElement.querySelector('.idle-dialog-continue'),
    secondaryButton: rootElement.querySelector('.idle-dialog-disconnect'),
    closeOnEscape: false,
    windowShape: windowShape
  });

  this.resetTimeout_();
};

remoting.IdleDetector.prototype.dispose = function() {
  base.dispose(this.timer_);
  this.timer_ = null;

  if (this.resetTimeoutRef_) {
    this.registerInputDetectionCallbacks_(false);
  }
  base.dispose(this.dialog_);
  this.dialog_ = null;
};

/**
 * @param {boolean} register True to register the callbacks; false to remove
 *     them.
 * @private
 */
remoting.IdleDetector.prototype.registerInputDetectionCallbacks_ =
    function(register) {
  var events = [ 'mousemove', 'mousedown', 'mouseup', 'click',
                 'keyup', 'keydown', 'keypress' ];
  if (register) {
    console.assert(this.resetTimeoutRef_ == null,
                   '|resetTimeoutRef_| already exists.');
    this.resetTimeoutRef_ = this.resetTimeout_.bind(this);
    for (var i = 0; i < events.length; ++i) {
      document.body.addEventListener(events[i], this.resetTimeoutRef_, true);
    }
  } else {
    console.assert(this.resetTimeoutRef_ != null,
                   '|resetTimeoutRef_| does not exist.');
    for (var i = 0; i < events.length; ++i) {
      document.body.removeEventListener(events[i], this.resetTimeoutRef_, true);
    }
    this.resetTimeoutRef_ = null;
  }
};

/**
 * @private
 */
remoting.IdleDetector.prototype.resetTimeout_ = function() {
  if (this.resetTimeoutRef_ == null) {
    this.registerInputDetectionCallbacks_(true);
  }
  base.dispose(this.timer_);
  this.timer_ = new base.OneShotTimer(this.onIdleTimeout_.bind(this),
                                      remoting.IdleDetector.kIdleTimeoutMs);
};

/**
 * @private
 */
remoting.IdleDetector.prototype.onIdleTimeout_ = function() {
  this.registerInputDetectionCallbacks_(false);
  this.timer_ = new base.OneShotTimer(this.onDialogTimeout_.bind(this),
                                      remoting.IdleDetector.kDialogTimeoutMs);
  this.showIdleWarning_();
};

/**
 * @private
 */
remoting.IdleDetector.prototype.onDialogTimeout_ = function() {
  base.dispose(this.timer_);
  this.timer_ = null;
  this.dialog_.close(remoting.MessageDialog.Result.SECONDARY);
};

/**
 * @private
 */
remoting.IdleDetector.prototype.showIdleWarning_ = function() {
  var that = this;
  this.dialog_.show().then(function(
      /** remoting.MessageDialog.Result */ result) {
    if (result === remoting.MessageDialog.Result.PRIMARY) {
      // Continue.
      that.resetTimeout_();
    } else if (result === remoting.MessageDialog.Result.SECONDARY) {
      // Disconnect.
      base.dispose(that.timer_);
      that.timer_ = null;
      that.callback_();
    }
  });
};

// Time-out after 1hr of no activity.
remoting.IdleDetector.kIdleTimeoutMs = 60 * 60 * 1000;

// Show the idle warning dialog for 2 minutes.
remoting.IdleDetector.kDialogTimeoutMs = 2 * 60 * 1000;
