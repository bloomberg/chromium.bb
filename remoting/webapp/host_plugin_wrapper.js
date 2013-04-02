// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a compatibility wrapper around the NPAPI plugin object,
// exposing an interface that matches the Native Messaging interface.

// TODO(lambroslambrou): Add error callback parameters here, and in Native
// Messaging. The extra callback parameters will be ignored here.

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {remoting.HostPlugin} plugin Wrapped NPAPI plugin object.
 * @extends remoting.HostNativeMessaging
 */
remoting.HostPluginWrapper = function(plugin) {
  /** @type {remoting.HostPlugin} @private */
  this.plugin_ = plugin;
};

/**
 * @param {function(string):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.getHostName = function(callback) {
  this.plugin_.getHostName(callback);
};

/**
 * @param {string} hostId
 * @param {string} pin
 * @param {function(string):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.getPinHash = function(hostId, pin,
                                                           callback) {
  this.plugin_.getPinHash(hostId, pin, callback);
};

/**
 * @param {function(string, string):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.generateKeyPair = function(callback) {
  this.plugin_.generateKeyPair(callback);
};

/**
 * @param {string} config
 * @param {function(remoting.HostController.AsyncResult):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.updateDaemonConfig = function(config,
                                                                   callback) {
  this.plugin_.updateDaemonConfig(config, callback);
};

/**
 * @param {function(string):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.getDaemonConfig = function(callback) {
  this.plugin_.getDaemonConfig(callback);
};

/**
 * @param {function(string):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.getDaemonVersion = function(callback) {
  this.plugin_.getDaemonVersion(callback);
};

/**
 * @param {function(boolean, boolean, boolean):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.getUsageStatsConsent = function(callback) {
  this.plugin_.getUsageStatsConsent(callback);
};

/**
 * @param {string} config
 * @param {function(remoting.HostController.AsyncResult):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.startDaemon = function(
    config, consent, callback) {
  this.plugin_.startDaemon(config, consent, callback);
};

/**
 * @param {function(remoting.HostController.AsyncResult):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.stopDaemon = function(callback) {
  this.plugin_.stopDaemon(callback);
};

/**
 * @param {function(remoting.HostController.State):void} callback
 * @return {void}
 */
remoting.HostPluginWrapper.prototype.getDaemonState = function(callback) {
  // Call the callback directly, since NPAPI exposes the state directly as a
  // field member, rather than an asynchronous method.
  var state = this.plugin_.daemonState;
  if (state === undefined) {
    // If the plug-in can't be instantiated, for example on ChromeOS, then
    // return something sensible.
    state = remoting.HostController.State.NOT_IMPLEMENTED;
  }
  callback(state);
}
