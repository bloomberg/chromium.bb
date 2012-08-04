// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to detect when the device is suspended, for example when a laptop's
 * lid is closed.
 */


'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {function():void} callback Callback function to invoke when a
 * suspend+resume operation has been detected.
 *
 * @constructor
 */
remoting.SuspendMonitor = function (callback) {
  /** @type {function():void} @private */
  this.callback_ = callback;
  /** @type {number} @private */
  this.timerIntervalMs_ = 60 * 1000;
  /** @type {number} @private */
  this.lateToleranceMs_ = 60 * 1000;
  /** @type {number} @private */
  this.callbackExpectedTime_ = 0;
  this.start_();
};

/** @private */
remoting.SuspendMonitor.prototype.start_ = function() {
  window.setTimeout(this.checkSuspend_.bind(this), this.timerIntervalMs_);
  this.callbackExpectedTime_ = new Date().getTime() + this.timerIntervalMs_;
};

/** @private */
remoting.SuspendMonitor.prototype.checkSuspend_ = function() {
  var lateByMs = new Date().getTime() - this.callbackExpectedTime_;
  if (lateByMs > this.lateToleranceMs_) {
    this.callback_();
  }
  this.start_();
};

/** @type {remoting.SuspendMonitor?} */
remoting.suspendMonitor = null;