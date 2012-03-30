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
  INSTALLING: 1,
  STOPPED: 2,
  STARTING: 3,
  STARTED: 4,
  STOPPING: 5,
  START_FAILED: 6,
  UNKNOWN: 7
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
  // TODO(sergeyu): This code updates UI state. Does it belong here,
  // or should it moved somewhere else?
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
  var element = document.getElementById('daemon-controller-ui');
  if (match == 'enabled') {
    element.classList.remove('host-offline');
  } else {
    element.classList.add('host-offline');
  }
};

/**
 * Generates new host key pair.
 * @param {function(string,string):void} callback Callback for the
 * generated key pair.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.generateKeyPair = function(callback) {
  this.plugin_.generateKeyPair(callback);
};

/**
 * @return {string} Local hostname
 */
remoting.DaemonPlugin.prototype.getHostName = function() {
  return this.plugin_.getHostName();
};

/**
 * Read current host configuration.
 * @param {function(string):void} callback Host config callback.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.getConfig = function(callback) {
  this.plugin_.getDaemonConfig(callback);
};

/**
 * Start the daemon process.
 * @param {string} config Host config.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.start = function(config) {
  this.plugin_.startDaemon(config);
};

/**
 * Stop the daemon process.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.stop = function() {
  this.plugin_.stopDaemon();
};

/**
 * @param {string} pin The new PIN for the daemon process.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.setPin = function(pin) {
  this.plugin_.setDaemonPin(pin);
};

/** @type {remoting.DaemonPlugin} */
remoting.daemonPlugin = null;
