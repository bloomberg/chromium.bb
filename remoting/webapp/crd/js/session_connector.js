// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Interface abstracting the SessionConnector functionality.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @interface
 */
remoting.SessionConnector = function() {};

/**
 * Initiate a Me2Me connection.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @param {function(boolean, function(string):void):void} fetchPin Function to
 *     interactively obtain the PIN from the user.
 * @param {function(string, string, string,
 *                  function(string, string): void): void}
 *     fetchThirdPartyToken Function to obtain a token from a third party
 *     authentication server.
 * @param {string} clientPairingId The client id issued by the host when
 *     this device was paired, if it is already paired.
 * @param {string} clientPairedSecret The shared secret issued by the host when
 *     this device was paired, if it is already paired.
 * @return {void} Nothing.
 */
remoting.SessionConnector.prototype.connectMe2Me =
    function(host, fetchPin, fetchThirdPartyToken,
             clientPairingId, clientPairedSecret) {};

/**
 * Retry connecting to a Me2Me host after a connection failure.
 *
 * @param {remoting.Host} host The Me2Me host to refresh.
 * @return {void} Nothing.
 */
remoting.SessionConnector.prototype.retryConnectMe2Me = function(host) {};

/**
 * Initiate a Me2App connection.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @param {function(string, string, string,
 *                  function(string, string): void): void}
 *     fetchThirdPartyToken Function to obtain a token from a third party
 *     authentication server.
 * @return {void} Nothing.
 */
remoting.SessionConnector.prototype.connectMe2App =
    function(host, fetchThirdPartyToken) {};

/**
 * Update the pairing info so that the reconnect function will work correctly.
 *
 * @param {string} clientId The paired client id.
 * @param {string} sharedSecret The shared secret.
 */
remoting.SessionConnector.prototype.updatePairingInfo =
    function(clientId, sharedSecret) {};

/**
 * Initiates a remote connection.
 *
 * @param {remoting.DesktopConnectedView.Mode} mode
 * @param {remoting.Host} host
 * @param {remoting.CredentialsProvider} credentialsProvider
 * @return {void} Nothing.
 */
remoting.SessionConnector.prototype.connect =
    function(mode, host, credentialsProvider) {};

/**
 * Reconnect a closed connection.
 *
 * @return {void} Nothing.
 */
remoting.SessionConnector.prototype.reconnect = function() {};

/**
 * Cancel a connection-in-progress.
 */
remoting.SessionConnector.prototype.cancel = function() {};

/**
 * Get the connection mode (Me2Me or IT2Me)
 *
 * @return {remoting.DesktopConnectedView.Mode}
 */
remoting.SessionConnector.prototype.getConnectionMode = function() {};

/**
 * Get host ID.
 *
 * @return {string}
 */
remoting.SessionConnector.prototype.getHostId = function() {};

/**
 * @param {remoting.ProtocolExtension} extension
 */
remoting.SessionConnector.prototype.registerProtocolExtension =
    function(extension) {};


/**
 * @interface
 */
remoting.SessionConnectorFactory = function() {};

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ClientSession):void} onConnected Callback on
 *     success.
 * @param {function(!remoting.Error):void} onError Callback on error.
 * @param {function(string, string):boolean} appProtocolExtensionHandler The
 *     handler for the application's protocol extension messages. Returns true
 *     if a message is recognized; false otherwise.
 * @param {function(!remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {string} defaultRemapKeys The default set of key mappings to use
 *     in the client session.
 * @return {remoting.SessionConnector}
 */
remoting.SessionConnectorFactory.prototype.createConnector =
    function(clientContainer, onConnected, onError, appProtocolExtensionHandler,
             onConnectionFailed, requiredCapabilities, defaultRemapKeys) {};

/**
 * @type {remoting.SessionConnectorFactory}
 */
remoting.SessionConnector.factory = null;
