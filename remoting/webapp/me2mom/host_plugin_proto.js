// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

/** @type {number} */ remoting.HostPlugin.prototype.state;

/** @type {number} */ remoting.HostPlugin.prototype.REQUESTED_ACCESS_CODE;
/** @type {number} */ remoting.HostPlugin.prototype.RECEIVED_ACCESS_CODE;
/** @type {number} */ remoting.HostPlugin.prototype.CONNECTED;
/** @type {number} */ remoting.HostPlugin.prototype.DISCONNECTED;
/** @type {number} */ remoting.HostPlugin.prototype.AFFIRMING_CONNECTION;
/** @type {number} */ remoting.HostPlugin.prototype.ERROR;

/** @type {number} */ remoting.HostPlugin.prototype.accessCodeLifetime;

/** @type {string} */ remoting.HostPlugin.prototype.client;
