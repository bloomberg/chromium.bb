// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type definitions for various remoting classes.
// It is used only with JSCompiler to verify the type-correctness of our code.

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor
 *  @extends Event
 */
remoting.ClipboardData = function() {};

/** @type {Array.<string>} */
remoting.ClipboardData.prototype.types;

/** @type {function(string): string} */
remoting.ClipboardData.prototype.getData;

/** @type {function(string, string): void} */
remoting.ClipboardData.prototype.setData;

/** @constructor
 */
remoting.ClipboardEvent = function() {};

/** @type {remoting.ClipboardData} */
remoting.ClipboardEvent.prototype.clipboardData;

/** @type {function(): void} */
remoting.ClipboardEvent.prototype.preventDefault;


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

/** @param {function(string):void} callback Callback to be called with the
 *      local hostname.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.getHostName = function(callback) {};

/** @param {string} hostId The host ID.
 *  @param {string} pin The PIN.
 *  @param {function(string):void} callback Callback to be called with the hash
 *      encoded with Base64.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.getPinHash = function(hostId, pin, callback) {};

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

/** @param {function(boolean, boolean, boolean):void} callback Callback to be
 *      called with the consent.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.getUsageStatsConsent = function(callback) {};

/** @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.installHost = function(callback) {};

/** @param {string} config Host configuration.
 *  @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.startDaemon = function(
    config, consent, callback) {};

/** @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 *  @return {void} Nothing. */
remoting.HostPlugin.prototype.stopDaemon = function(callback) {};

/** @param {function(string):void} callback Callback to be called with the
 *      JSON-encoded list of paired clients.
 *  @return {void} Nothing.
 */
remoting.HostPlugin.prototype.getPairedClients = function(callback) {};

/** @param {function(boolean):void} callback Callback to be called when
 *      finished.
 *  @return {void} Nothing.
 */
remoting.HostPlugin.prototype.clearPairedClients = function(callback) {};

/** @param {string} client Client id of the pairing to be deleted.
 *  @param {function(boolean):void} callback Callback to be called when
 *      finished.
 *  @return {void} Nothing.
 */
remoting.HostPlugin.prototype.deletePairedClient = function(
    client, callback) {};

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

/** @type {string} */ remoting.HostPlugin.prototype.xmppServerAddress;
/** @type {boolean} */ remoting.HostPlugin.prototype.xmppServerUseTls;
/** @type {string} */ remoting.HostPlugin.prototype.directoryBotJid;
/** @type {string} */ remoting.HostPlugin.prototype.supportedFeatures;


/** @constructor
 *  @extends HTMLEmbedElement
 */
remoting.ViewerPlugin = function() { };

/** @param {string} message The message to send to the host. */
remoting.ViewerPlugin.prototype.postMessage = function(message) {};


/** @constructor
 */
remoting.WcsIqClient = function() {};

/** @param {function(Array.<string>): void} onMsg The function called when a
 *      message is received.
 *  @return {void} Nothing. */
remoting.WcsIqClient.prototype.setOnMessage = function(onMsg) {};

/** @return {void} Nothing. */
remoting.WcsIqClient.prototype.connectChannel = function() {};

/** @param {string} stanza An IQ stanza.
 *  @return {void} Nothing. */
remoting.WcsIqClient.prototype.sendIq = function(stanza) {};

/** @param {string} token An OAuth2 access token.
 *  @return {void} Nothing. */
remoting.WcsIqClient.prototype.updateAccessToken = function(token) {};
