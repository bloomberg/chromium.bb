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
 * Initiates a remote connection.
 *
 * @param {remoting.Application.Mode} mode
 * @param {remoting.Host} host
 * @param {remoting.CredentialsProvider} credentialsProvider
 * @param {boolean=} opt_suppressOfflineError  Suppress the host offline error
 *    as we use stale JID's when initiating a Me2Me.  This parameter will be
 *    removed when we get rid of sessionConnector altogether in a future CL.
 * @return {void} Nothing.
 */
remoting.SessionConnector.prototype.connect =
    function(mode, host, credentialsProvider, opt_suppressOfflineError) {};


/**
 * Closes the session and removes the plugin element.
 */
remoting.SessionConnector.prototype.closeSession = function() {};

/**
 * @interface
 */
remoting.SessionConnectorFactory = function() {};

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ConnectionInfo):void} onConnected Callback on
 *     success.
 * @param {function(!remoting.Error):void} onError Callback on error.
 * @param {function(!remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @return {remoting.SessionConnector}
 */
remoting.SessionConnectorFactory.prototype.createConnector =
    function(clientContainer, onConnected, onError,
             onConnectionFailed, requiredCapabilities) {};

/**
 * @type {remoting.SessionConnectorFactory}
 */
remoting.SessionConnector.factory = null;
