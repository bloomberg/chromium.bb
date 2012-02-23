// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor */
remoting.DaemonPlugin = function() {
  /** @type {remoting.HostPlugin} @private */
  this.plugin_ = remoting.HostSession.createPlugin();
  /** @type {HTMLElement} @private */
  this.container_ = document.getElementById('daemon-plugin-container');
  this.container_.appendChild(this.plugin_);
};

// Note that the values in this enum are copied from daemon_controller.h and
// must be kept in sync.
/** @enum {number} */
remoting.DaemonPlugin.State = {
  NOT_IMPLEMENTED: -1,
  NOT_INSTALLED: 0,
  STOPPED: 1,
  STARTED: 2,
  START_FAILED: 3,
  UNKNOWN: 4
};

/** @return {remoting.DaemonPlugin.State} The current state of the daemon. */
remoting.DaemonPlugin.prototype.state = function() {
  try {
    return this.plugin_.daemonState;
  } catch (err) {
    // If the plug-in can't be instantiated, for example on ChromeOS, then
    // return something sensible.
    return remoting.DaemonPlugin.State.NOT_IMPLEMENTED;
  }
};

/**
 * Show or hide daemon-specific parts of the UI.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.updateDom = function() {
  var match = '';
  switch (this.state()) {
    case remoting.DaemonPlugin.State.STARTED:
      match = 'enabled';
      break;
    case remoting.DaemonPlugin.State.STOPPED:
    case remoting.DaemonPlugin.State.NOT_INSTALLED:
    case remoting.DaemonPlugin.State.START_FAILED:
      match = 'disabled';
      break;
  }
  remoting.updateModalUi(match, 'data-daemon-state');
};

/**
 * Start the daemon process.
 * @return {boolean} False if insufficient state has been set.
 */
remoting.DaemonPlugin.prototype.start = function() {
  return this.plugin_.startDaemon();
};

/**
 * Stop the daemon process.
 * @return {boolean} False if insufficient state has been set.
 */
remoting.DaemonPlugin.prototype.stop = function() {
  return this.plugin_.stopDaemon();
};

/**
 * @param {string} pin The new PIN for the daemon process.
 * @return {boolean} True if the PIN was set successfully.
 */
remoting.DaemonPlugin.prototype.setPin = function(pin) {
  return this.plugin_.setDaemonPin(pin);
};

/** @type {remoting.DaemonPlugin} */
remoting.daemonPlugin = null;
