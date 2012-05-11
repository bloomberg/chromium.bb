// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type definitions for the host plugin. It is used only
// with JSCompiler to verify the type-correctness of our code.

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor
 *  @extends HTMLElement
 */
remoting.HostPlugin = function() {};

/** @param {string} email The email address of the connector.
 *  @param {string} token The access token for the connector.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.connect = function(email, token) {};

/** @return {void} Nothing. */
remoting.HostPlugin.prototype.disconnect = function() {};

/** @param {function(string):string} callback Pointer to chrome.i18n.getMessage.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.localize = function(callback) {};

/** @return {string} Local hostname. */
remoting.HostPlugin.prototype.getHostName = function() {};

/** @param {string} hostId The host ID.
 *  @param {string} pin The PIN.
 *  @return {string} The hash encoded with Base64. */
remoting.HostPlugin.prototype.getPinHash = function(hostId, pin) {};

/** @param {function(string, string):void} callback Callback to be called
 *  after new key is generated.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.generateKeyPair = function(callback) {};

/**
 * Updates host config with the values specified in |config|. All
 * fields that are not specified in |config| remain
 * unchanged. Following parameters cannot be changed using this
 * function: host_id, xmpp_login. Error is returned if |config|
 * includes these paramters. Changes take effect before the callback
 * is called.
 *
 * @param {string} config The new config parameters, JSON encoded dictionary.
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing. */
remoting.HostPlugin.prototype.updateDaemonConfig =
    function(config, callback) {};

/** @param {function(string):void} callback Callback to be called with
 *      the config.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.getDaemonConfig = function(callback) {};

/** @param {function(string):void} callback Callback to be called with
 *      the version, as a dotted string.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.getDaemonVersion = function(callback) {};

/** @param {string} config Host configuration.
 *  @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.startDaemon = function(config, callback) {};

/** @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.stopDaemon = function(callback) {};

/** @type {number} */ remoting.HostPlugin.prototype.state;

/** @type {number} */ remoting.HostPlugin.prototype.STARTING;
/** @type {number} */ remoting.HostPlugin.prototype.REQUESTED_ACCESS_CODE;
/** @type {number} */ remoting.HostPlugin.prototype.RECEIVED_ACCESS_CODE;
/** @type {number} */ remoting.HostPlugin.prototype.CONNECTED;
/** @type {number} */ remoting.HostPlugin.prototype.DISCONNECTED;
/** @type {number} */ remoting.HostPlugin.prototype.DISCONNECTING;
/** @type {number} */ remoting.HostPlugin.prototype.ERROR;

/** @type {string} */ remoting.HostPlugin.prototype.accessCode;
/** @type {number} */ remoting.HostPlugin.prototype.accessCodeLifetime;

/** @type {string} */ remoting.HostPlugin.prototype.client;

/** @type {remoting.HostController.State} */
remoting.HostPlugin.prototype.daemonState;

/** @type {function(boolean):void} */
remoting.HostPlugin.prototype.onNatTraversalPolicyChanged;
