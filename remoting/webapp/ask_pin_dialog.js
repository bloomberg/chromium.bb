// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.DaemonPlugin} daemon The parent daemon plugin instance.
 * @constructor
 */
remoting.AskPinDialog = function(daemon) {
  this.startDaemon_ = false;
  this.daemon_ = daemon;
  this.okButton_ = document.getElementById('daemon-pin-ok');
  this.spinner_ = document.getElementById('start-daemon-spinner');
  this.pinEntry_ = document.getElementById('daemon-pin-entry');
  this.pinConfirm_ = document.getElementById('daemon-pin-confirm');
  /** @type {remoting.AskPinDialog} */
  var that = this;
  /** @param {Event} event The event. */
  var onSubmit = function(event) {
    event.preventDefault();
    that.onSubmit_();
  };
  var form = document.getElementById('ask-pin-form');
  form.addEventListener('submit', onSubmit, false);
};

/**
 * Show the dialog in order to get a PIN prior to starting the daemon. When the
 * user clicks OK, the dialog shows a spinner until the daemon has started.
 *
 * @return {void} Nothing.
 */
remoting.AskPinDialog.prototype.showForStart = function() {
  remoting.setMode(remoting.AppMode.ASK_PIN);
  this.startDaemon_ = true;
};

/**
 * Show the dialog in order to change the PIN associated with a running daemon.
 *
 * @return {void} Nothing.
 */
remoting.AskPinDialog.prototype.showForPin = function() {
  remoting.setMode(remoting.AppMode.ASK_PIN);
  this.startDaemon_ = false;
};

/**
 * @return {void} Nothing.
 */
remoting.AskPinDialog.prototype.hide = function() {
  remoting.setMode(remoting.AppMode.HOME);
};

/** @private */
remoting.AskPinDialog.prototype.onSubmit_ = function() {
  // TODO(jamiewalch): Add validation and error checks when we improve the UI.
  var pin = this.pinEntry_.value;
  this.daemon_.setPin(pin);
  if (this.startDaemon_) {
    this.daemon_.start();
    this.pollDaemonState_();
  } else {
    this.hide();
  }
};

/**
 * @return {void} Nothing.
 * @private
 */
remoting.AskPinDialog.prototype.pollDaemonState_ = function() {
  var state = this.daemon_.state();
  var retry = false;  // Set to true if we haven't finished yet.
  switch (state) {
    case remoting.DaemonPlugin.State.STOPPED:
    case remoting.DaemonPlugin.State.NOT_INSTALLED:
      retry = true;
      break;
    case remoting.DaemonPlugin.State.STARTED:
      this.hide();
      this.daemon_.updateDom();
      break;
    case remoting.DaemonPlugin.State.START_FAILED:
      // TODO(jamiewalch): Show an error message.
      break;
    default:
      // TODO(jamiewalch): Show an error message.
      console.error('Unexpected daemon state', state);
      break;
  }
  if (retry) {
    this.okButton_.hidden = true;
    this.spinner_.hidden = false;
    /** @type {remoting.AskPinDialog} */
    var that = this;
    var pollDaemonState = function() { that.pollDaemonState_(); }
    window.setTimeout(pollDaemonState, 1000);
  } else {
    this.okButton_.hidden = false;
    this.spinner_.hidden = true;
  }
};

/** @type {remoting.AskPinDialog} */
remoting.askPinDialog = null;
