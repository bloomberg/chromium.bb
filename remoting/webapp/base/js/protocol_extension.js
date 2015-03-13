// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Interface abstracting the protocol extension functionality.
 * Instances of this class can be registered with the SessionConnector
 * to enhance the communication protocol between the host and client.
 * Note that corresponding support on the host side is required.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @interface
 */
remoting.ProtocolExtension = function() {};

/**
 * The string that identifies the type of the extension.
 * All extension messages with this type will be sent to the extension.
 *
 * @return {string}
 */
remoting.ProtocolExtension.prototype.getType = function() {};

/**
 * Called when the connection has been established to start the extension.
 *
 * @param {function(string,string)} sendMessageToHost Callback to send a message
 *     to the host.
 */
remoting.ProtocolExtension.prototype.start =
    function(sendMessageToHost) {};

/**
 * Called when an extension message of a matching type is received.
 *
 * @param {string} message
 */
remoting.ProtocolExtension.prototype.onMessage =
    function(message) {};
