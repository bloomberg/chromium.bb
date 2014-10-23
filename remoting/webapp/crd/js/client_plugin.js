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
 * @return {number} The width of the remote desktop, in pixels.
 */
remoting.ClientPlugin.prototype.getDesktopWidth = function() {};

/**
 * @return {number} The height of the remote desktop, in pixels.
 */
remoting.ClientPlugin.prototype.getDesktopHeight = function() {};

/**
 * @return {number} The x-DPI of the remote desktop.
 */
remoting.ClientPlugin.prototype.getDesktopXDpi = function() {};

/**
 * @return {number} The y-DPI of the remote desktop.
 */
remoting.ClientPlugin.prototype.getDesktopYDpi = function() {};

/**
 * @return {HTMLElement} The DOM element representing the remote session.
 */
remoting.ClientPlugin.prototype.element = function() {};

/**
 * @param {function():void} onDone Completion callback.
 */
remoting.ClientPlugin.prototype.initialize = function(onDone) {};

/**
 * @param {string} hostJid The jid of the host to connect to.
 * @param {string} hostPublicKey The base64 encoded version of the host's
 *     public key.
 * @param {string} localJid Local jid.
 * @param {string} sharedSecret The access code for IT2Me or the PIN
 *     for Me2Me.
 * @param {string} authenticationMethods Comma-separated list of
 *     authentication methods the client should attempt to use.
 * @param {string} authenticationTag A host-specific tag to mix into
 *     authentication hashes.
 * @param {string} clientPairingId For paired Me2Me connections, the
 *     pairing id for this client, as issued by the host.
 * @param {string} clientPairedSecret For paired Me2Me connections, the
 *     paired secret for this client, as issued by the host.
 */
remoting.ClientPlugin.prototype.connect = function(
    hostJid, hostPublicKey, localJid, sharedSecret,
    authenticationMethods, authenticationTag,
    clientPairingId, clientPairedSecret) {};

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
 * @param {number} width
 * @param {number} height
 * @param {number} dpi
 */
remoting.ClientPlugin.prototype.notifyClientResolution =
    function(width, height, dpi) {};

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
 * @return {boolean} True if the plugin support the specified feature.
 */
remoting.ClientPlugin.prototype.hasFeature = function(feature) {};

/**
 * Enable MediaSource rendering via the specified renderer.
 *
 * @param {remoting.MediaSourceRenderer} mediaSourceRenderer
 */
remoting.ClientPlugin.prototype.enableMediaSourceRendering =
    function(mediaSourceRenderer) {};

/**
 * Sends a clipboard item to the host.
 *
 * @param {string} mimeType The MIME type of the clipboard item.
 * @param {string} item The clipboard item.
 */
remoting.ClientPlugin.prototype.sendClipboardItem =
    function(mimeType, item) {};

/**
 * Tell the plugin to request a PIN asynchronously.
 */
remoting.ClientPlugin.prototype.useAsyncPinDialog = function() {};

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
 * Called when a PIN is obtained from the user.
 *
 * @param {string} pin The PIN.
 */
remoting.ClientPlugin.prototype.onPinFetched = function(pin) {};

/**
 * Sets the third party authentication token and shared secret.
 *
 * @param {string} token The token received from the token URL.
 * @param {string} sharedSecret Shared secret received from the token URL.
 */
remoting.ClientPlugin.prototype.onThirdPartyTokenFetched =
    function(token, sharedSecret) {};

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
 * @param {function(string):void} handler Callback for sending an IQ stanza.
 */
remoting.ClientPlugin.prototype.setOnOutgoingIqHandler =
    function(handler) {};

/**
 * @param {function(string):void} handler Callback for logging debug messages.
 */
remoting.ClientPlugin.prototype.setOnDebugMessageHandler =
    function(handler) {};

/**
 * @param {function(number, number):void} handler Callback for connection status
 *     update notifications. The first parameter is the connection state; the
 *     second is the error code, if any.
 */
remoting.ClientPlugin.prototype.setConnectionStatusUpdateHandler =
    function(handler) {};

/**
 * @param {function(boolean):void} handler Callback for connection readiness
 *     notifications.
 */
remoting.ClientPlugin.prototype.setConnectionReadyHandler =
    function(handler) {};

/**
 * @param {function():void} handler Callback for desktop size change
 *     notifications.
 */
remoting.ClientPlugin.prototype.setDesktopSizeUpdateHandler =
    function(handler) {};

/**
 * @param {function(!Array.<string>):void} handler Callback to inform of
 *     capabilities negotiated between host and client.
 */
remoting.ClientPlugin.prototype.setCapabilitiesHandler =
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
 * @param {function(string, string, string):void} handler Callback for
 *     fetching third-party tokens. The first parameter is the token URL; the
 *     second is the public key of the host; the third is the OAuth2 scope
 *     being requested.
 */
remoting.ClientPlugin.prototype.setFetchThirdPartyTokenHandler =
    function(handler) {};

/**
 * @param {function(boolean):void} handler Callback for fetching a PIN from
 *     the user. The parameter is true if PIN pairing is supported by the
 *     host, or false otherwise.
 */
remoting.ClientPlugin.prototype.setFetchPinHandler =
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
  EXTENSION_MESSAGE: 'extensionMessage',
  MEDIA_SOURCE_RENDERING: 'mediaSourceRendering',
  VIDEO_CONTROL: 'videoControl'
};


/**
 * @interface
 */
remoting.ClientPluginFactory = function() {};

/**
 * @param {Element} container The container for the embed element.
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @return {remoting.ClientPlugin} A new client plugin instance.
 */
remoting.ClientPluginFactory.prototype.createPlugin =
    function(container, onExtensionMessage) {};

/**
 * Preload the plugin to make instantiation faster when the user tries
 * to connect.
 */
remoting.ClientPluginFactory.prototype.preloadPlugin = function() {};

/**
 * @type {remoting.ClientPluginFactory}
 */
remoting.ClientPlugin.factory = null;
