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
  /** @type {remoting.Host?} */
  this.localHost = null;
};

// Note that the values in the enums below are copied from
// daemon_controller.h and must be kept in sync.
/** @enum {number} */
remoting.DaemonPlugin.State = {
  NOT_IMPLEMENTED: -1,
  NOT_INSTALLED: 0,
  INSTALLING: 1,
  STOPPED: 2,
  STARTING: 3,
  STARTED: 4,
  STOPPING: 5,
  UNKNOWN: 6
};

/** @enum {number} */
remoting.DaemonPlugin.AsyncResult = {
  OK: 0,
  FAILED: 1,
  CANCELLED: 2
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
      match = 'disabled';
      break;
  }
  remoting.updateModalUi(match, 'data-daemon-state');
  var element = document.getElementById('this-host-connect');
  if (match == 'enabled') {
    element.classList.remove('host-offline');
    element.classList.add('clickable');
  } else {
    element.classList.add('host-offline');
    element.classList.remove('clickable');
    element.title = '';
  }
};

/**
 * Set tool-tips for the 'connect' action. We can't just set this on the
 * parent element because the button has no tool-tip, and therefore would
 * inherit connectStr.
 *
 * return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.setTooltips = function() {
  var connectStr = '';
  if (this.localHost) {
    chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_CONNECT',
                           this.localHost.hostName);
  }
  document.getElementById('this-host-name').title = connectStr;
  document.getElementById('this-host-icon').title = connectStr;
  document.getElementById('this-host-spacer').title = connectStr;
};

/**
 * Generates new host key pair.
 * @param {function(string,string):void} callback Callback for the
 *     generated key pair.
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
 * @param {function(remoting.DaemonPlugin.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.start = function(config, callback) {
  this.plugin_.startDaemon(config, callback);
};

/**
 * Stop the daemon process.
 * @param {function(remoting.DaemonPlugin.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.stop = function(callback) {
  this.plugin_.stopDaemon(callback);
};

/**
 * @param {string} config The new host config, JSON encoded.
 * @param {function(remoting.DaemonPlugin.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.updateConfig = function(config, callback) {
  this.plugin_.updateDaemonConfig(config, callback);
};

/**
 * Set the remoting.Host instance (retrieved from the Chromoting service)
 * that corresponds to the local computer, if any.
 *
 * @param {remoting.Host?} host The host, or null if not registered.
 * @return {void} Nothing.
 */
remoting.DaemonPlugin.prototype.setHost = function(host) {
  this.localHost = host;
  this.setTooltips();
  /** @type {remoting.DaemonPlugin} */
  var that = this;
  if (host) {
    /** @param {remoting.HostTableEntry} host */
    var renameHost = function(host) {
      remoting.hostList.renameHost(host);
      that.setTooltips();
    };
    /** @type {remoting.HostTableEntry} @private */
    this.hostTableEntry_ = new remoting.HostTableEntry();
    this.hostTableEntry_.init(host,
                              document.getElementById('this-host-connect'),
                              document.getElementById('this-host-name'),
                              document.getElementById('this-host-rename'),
                              renameHost);
  } else {
    this.hostTableEntry_ = null;
  }
};

/**
 * Update the internal state so that the local host can be correctly filtered
 * out of the host list.
 *
 * @param {remoting.HostList} hostList The new host list, returned by the
 *     Chromoting service.
 * @param {function():void} onDone Completion callback. TODO(jamiewalch): For
 *     now, this is synchronous and reads the host id from local storage. In
 *     the future, it will asynchronously read the host id from the plugin
 *     (crbug.com/121518).
 */
remoting.DaemonPlugin.prototype.onHostListRefresh = function(hostList, onDone) {
  var hostId = window.localStorage.getItem('me2me-host-id');
  if (hostId && typeof(hostId) == 'string') {
    this.setHost(hostList.getHostForId(/** @type{string} */(hostId)));
  } else {
    this.setHost(null);
  }
  onDone();
};

/** @type {remoting.DaemonPlugin} */
remoting.daemonPlugin = null;
