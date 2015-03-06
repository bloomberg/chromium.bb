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
 * @param {HTMLElement} idleWarning The idle warning dialog.
 * @param {function():void} callback Called when the idle warning dialog has
 *     timed out or the user has explicitly indicated that they are no longer
 *     using the session.
 * @constructor
 * @implements {remoting.WindowShape.ClientUI}
 */
remoting.IdleDetector = function(idleWarning, callback) {
  /** @private */
  this.idleWarning_ = idleWarning;

  /** @private */
  this.callback_ = callback;

  /**
   * @private {number?} The id of the running timer, or null if no timer is
   *     running.
   */
  this.timerId_ = null;

  /** @private {?function():void} */
  this.resetTimeoutRef_ = null;

  var manifest = chrome.runtime.getManifest();
  var message = this.idleWarning_.querySelector('.idle-warning-message');
  l10n.localizeElement(message, manifest.name);

  var cont = this.idleWarning_.querySelector('.idle-dialog-continue');
  cont.addEventListener('click', this.onContinue_.bind(this), false);
  var quit = this.idleWarning_.querySelector('.idle-dialog-disconnect');
  quit.addEventListener('click', this.onDisconnect_.bind(this), false);

  remoting.windowShape.addCallback(this);
  this.resetTimeout_();
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
    base.debug.assert(this.resetTimeoutRef_ == null);
    this.resetTimeoutRef_ = this.resetTimeout_.bind(this);
    for (var i = 0; i < events.length; ++i) {
      document.body.addEventListener(events[i], this.resetTimeoutRef_, true);
    }
  } else {
    base.debug.assert(this.resetTimeoutRef_ != null);
    for (var i = 0; i < events.length; ++i) {
      document.body.removeEventListener(events[i], this.resetTimeoutRef_, true);
    }
    this.resetTimeoutRef_ = null;
  }
}

/**
 * @private
 */
remoting.IdleDetector.prototype.resetTimeout_ = function() {
  if (this.timerId_ !== null) {
    window.clearTimeout(this.timerId_);
  }
  if (this.resetTimeoutRef_ == null) {
    this.registerInputDetectionCallbacks_(true);
  }
  this.timerId_ = window.setTimeout(this.onIdleTimeout_.bind(this),
                                    remoting.IdleDetector.kIdleTimeoutMs);
};

/**
 * @private
 */
remoting.IdleDetector.prototype.onIdleTimeout_ = function() {
  this.registerInputDetectionCallbacks_(false);
  this.showIdleWarning_(true);
  this.timerId_ = window.setTimeout(this.onDialogTimeout_.bind(this),
                                    remoting.IdleDetector.kDialogTimeoutMs);
};

/**
 * @private
 */
remoting.IdleDetector.prototype.onDialogTimeout_ = function() {
  this.timerId_ = null;
  this.showIdleWarning_(false);
  this.callback_();
};

/**
 * @private
 */
remoting.IdleDetector.prototype.onContinue_ = function() {
  this.showIdleWarning_(false);
  this.resetTimeout_();
};

/**
 * @private
 */
remoting.IdleDetector.prototype.onDisconnect_ = function() {
  if (this.timerId_ !== null) {
    window.clearTimeout(this.timerId_);
  }
  this.onDialogTimeout_();
};

/**
 * @param {boolean} show True to show the warning dialog; false to hide it.
 * @private
 */
remoting.IdleDetector.prototype.showIdleWarning_ = function(show) {
  this.idleWarning_.hidden = !show;
  remoting.windowShape.updateClientWindowShape();
}

/**
 * @param {Array<{left: number, top: number, width: number, height: number}>}
 *     rects List of rectangles.
 */
remoting.IdleDetector.prototype.addToRegion = function(rects) {
  if (!this.idleWarning_.hidden) {
    var dialog = this.idleWarning_.querySelector('.kd-modaldialog');
    var rect = /** @type {ClientRect} */ (dialog.getBoundingClientRect());
    rects.push(rect);
  }
};

// Time-out after 1hr of no activity.
remoting.IdleDetector.kIdleTimeoutMs = 60 * 60 * 1000;

// Show the idle warning dialog for 2 minutes.
remoting.IdleDetector.kDialogTimeoutMs = 2 * 60 * 1000;
