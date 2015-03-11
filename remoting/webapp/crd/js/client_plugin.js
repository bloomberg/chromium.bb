// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Interface abstracting the ClientPlugin functionality.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @interface
 * @extends {base.Disposable}
 */
remoting.ClientPlugin = function() {};

/**
 * @return {remoting.HostDesktop}
 */
remoting.ClientPlugin.prototype.hostDesktop = function() {};

/**
 * @return {HTMLElement} The DOM element representing the remote session.
 */
remoting.ClientPlugin.prototype.element = function() {};

/**
 * @param {function(boolean):void} onDone Completion callback.
 */
remoting.ClientPlugin.prototype.initialize = function(onDone) {};

/**
 * @param {remoting.Host} host The host to connect to.
 * @param {string} localJid Local jid.
 * @param {remoting.CredentialsProvider} credentialsProvider
 */
remoting.ClientPlugin.prototype.connect =
    function(host, localJid, credentialsProvider) {};

/**
 * @param {number} key The keycode to inject.
 * @param {boolean} down True for press; false for a release.
 */
remoting.ClientPlugin.prototype.injectKeyEvent =
    function(key, down) {};

/**
 * @param {number} from
 * @param {number} to
 */
remoting.ClientPlugin.prototype.remapKey = function(from, to) {};

/**
 * Release all keys currently being pressed.
 */
remoting.ClientPlugin.prototype.releaseAllKeys = function() {};

/**
 * @param {string} iq
 */
remoting.ClientPlugin.prototype.onIncomingIq = function(iq) {};

/**
 * @return {boolean} True if the web-app and plugin are compatible.
 */
remoting.ClientPlugin.prototype.isSupportedVersion = function() {};

/**
 * @param {remoting.ClientPlugin.Feature} feature
 * @return {boolean} True if the plugin supports the specified feature.
 */
remoting.ClientPlugin.prototype.hasFeature = function(feature) {};

/**
 * Sends a clipboard item to the host.
 *
 * @param {string} mimeType The MIME type of the clipboard item.
 * @param {string} item The clipboard item.
 */
remoting.ClientPlugin.prototype.sendClipboardItem =
    function(mimeType, item) {};

/**
 * Request that this client be paired with the current host.
 *
 * @param {string} clientName The human-readable name of the client.
 * @param {function(string, string):void} onDone Callback to receive the
 *     client id and shared secret when they are available.
 */
remoting.ClientPlugin.prototype.requestPairing =
    function(clientName, onDone) {};

/**
 * Allows automatic mouse-lock.
 */
remoting.ClientPlugin.prototype.allowMouseLock = function() {};

/**
 * @param {boolean} pause True to pause the audio stream; false to resume it.
 */
remoting.ClientPlugin.prototype.pauseAudio = function(pause) {};

/**
 * @param {boolean} pause True to pause the video stream; false to resume it.
 */
remoting.ClientPlugin.prototype.pauseVideo = function(pause) {};

/**
 * @return {remoting.ClientSession.PerfStats} A summary of the connection
 *     performance.
 */
remoting.ClientPlugin.prototype.getPerfStats = function() {};

/**
 * Send an extension message to the host.
 *
 * @param {string} name
 * @param {string} data
 */
remoting.ClientPlugin.prototype.sendClientMessage =
    function(name, data) {};

/**
 * @param {remoting.ClientPlugin.ConnectionEventHandler} handler
 */
remoting.ClientPlugin.prototype.setConnectionEventHandler =
    function(handler) {};

/**
 * @param {function(string):void} handler Callback for processing security key
 *     (Gnubby) protocol messages.
 */
remoting.ClientPlugin.prototype.setGnubbyAuthHandler =
    function(handler) {};

/**
 * @param {function(string):void} handler Callback for processing Cast protocol
 *     messages.
 */
remoting.ClientPlugin.prototype.setCastExtensionHandler =
    function(handler) {};

/**
 * @param {function(string, number, number):void} handler Callback for
 *     processing large mouse cursor images. The first parameter is a data:
 *     URL encoding the mouse cursor; the second and third parameters are
 *     the cursor hotspot's x- and y-coordinates, respectively.
 */
remoting.ClientPlugin.prototype.setMouseCursorHandler =
    function(handler) {};

/**
 * @param {function({rects:Array<Array<number>>}):void|null} handler Callback
 *     to receive dirty region information for each video frame, for debugging.
 */
remoting.ClientPlugin.prototype.setDebugDirtyRegionHandler =
    function(handler) {};


/**
 * Set of features for which hasFeature() can be used to test.
 *
 * @enum {string}
 */
remoting.ClientPlugin.Feature = {
  INJECT_KEY_EVENT: 'injectKeyEvent',
  NOTIFY_CLIENT_RESOLUTION: 'notifyClientResolution',
  ASYNC_PIN: 'asyncPin',
  PAUSE_VIDEO: 'pauseVideo',
  PAUSE_AUDIO: 'pauseAudio',
  REMAP_KEY: 'remapKey',
  SEND_CLIPBOARD_ITEM: 'sendClipboardItem',
  THIRD_PARTY_AUTH: 'thirdPartyAuth',
  TRAP_KEY: 'trapKey',
  PINLESS_AUTH: 'pinlessAuth',
  ALLOW_MOUSE_LOCK: 'allowMouseLock',
  EXTENSION_MESSAGE: 'extensionMessage',
  VIDEO_CONTROL: 'videoControl'
};


/**
 * @interface
 */
remoting.ClientPlugin.ConnectionEventHandler = function() {};

/**
 * @param {string} iq
 */
remoting.ClientPlugin.ConnectionEventHandler.prototype.onOutgoingIq =
    function(iq) {};

/**
 * @param {string} msg
 */
remoting.ClientPlugin.ConnectionEventHandler.prototype.onDebugMessage =
    function(msg) {};

/**
 * @param {remoting.ClientSession.State} status The plugin's status.
 * @param {remoting.ClientSession.ConnectionError} error The plugin's error
 *        state, if any.
 */
remoting.ClientPlugin.ConnectionEventHandler.prototype.
    onConnectionStatusUpdate = function(status, error) {};

/**
 * @param {string} channel The channel name.
 * @param {string} connectionType The new connection type.
 */
remoting.ClientPlugin.ConnectionEventHandler.prototype.onRouteChanged =
    function(channel, connectionType) {};

/**
 * @param {boolean} ready True if the connection is ready.
 */
remoting.ClientPlugin.ConnectionEventHandler.prototype.onConnectionReady =
    function(ready) {};

/**
 * @param {!Array<string>} capabilities The set of capabilities negotiated
 *     between the client and host.
 */
remoting.ClientPlugin.ConnectionEventHandler.prototype.onSetCapabilities =
    function(capabilities) {};


/**
 * @interface
 */
remoting.ClientPluginFactory = function() {};

/**
 * @param {Element} container The container for the embed element.
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @param {Array<string>} requiredCapabilities
 * @return {remoting.ClientPlugin} A new client plugin instance.
 */
remoting.ClientPluginFactory.prototype.createPlugin =
    function(container, onExtensionMessage, requiredCapabilities) {};

/**
 * Preload the plugin to make instantiation faster when the user tries
 * to connect.
 */
remoting.ClientPluginFactory.prototype.preloadPlugin = function() {};

/**
 * @type {remoting.ClientPluginFactory}
 */
remoting.ClientPlugin.factory = null;
