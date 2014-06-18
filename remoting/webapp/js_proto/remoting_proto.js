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
