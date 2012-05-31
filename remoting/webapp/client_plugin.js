// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Interface used for ClientPlugin objects.
 * @interface
 */
remoting.ClientPlugin = function() {
};

/** @type {number} Desktop width */
remoting.ClientPlugin.prototype.desktopWidth;
/** @type {number} Desktop height */
remoting.ClientPlugin.prototype.desktopHeight;

/** @type {function(string): void} Outgoing signaling message callback. */
remoting.ClientPlugin.prototype.onOutgoingIqHandler;
/** @type {function(string): void} Debug messages callback. */
remoting.ClientPlugin.prototype.onDebugMessageHandler;
/** @type {function(number, number): void} State change callback. */
remoting.ClientPlugin.prototype.onConnectionStatusUpdateHandler;
/** @type {function(): void} Desktop size change callback. */
remoting.ClientPlugin.prototype.onDesktopSizeUpdateHandler;

/**
 * Initializes the plugin asynchronously and calls specified function
 * when done.
 *
 * @param {function(boolean): void} onDone Function to be called when
 * the plugin is initialized. Parameter is set to true when the plugin
 * is loaded successfully.
 */
remoting.ClientPlugin.prototype.initialize = function(onDone) {};

/**
 * @return {boolean} True if the plugin and web-app versions are compatible.
 */
remoting.ClientPlugin.prototype.isSupportedVersion = function() {};

/**
 * Set of features for which hasFeature() can be used to test.
 *
 * @enum {string}
 */
remoting.ClientPlugin.Feature = {
  HIGH_QUALITY_SCALING: 'highQualityScaling',
  INJECT_KEY_EVENT: 'injectKeyEvent',
  NOTIFY_CLIENT_DIMENSIONS: 'notifyClientDimensions',
  PAUSE_VIDEO: 'pauseVideo',
  REMAP_KEY: 'remapKey',
  SEND_CLIPBOARD_ITEM: 'sendClipboardItem'
};

/**
 * @param {remoting.ClientPlugin.Feature} feature The feature to test for.
 * @return {boolean} True if the plugin supports the named feature.
 */
remoting.ClientPlugin.prototype.hasFeature = function(feature) {};

/**
 * @return {HTMLEmbedElement} HTML element that corresponds to the plugin.
 */
remoting.ClientPlugin.prototype.element = function() {};

/**
 * Deletes the plugin.
 */
remoting.ClientPlugin.prototype.cleanup = function() {};

/**
 * Must be called for each incoming stanza received from the host.
 * @param {string} iq Incoming IQ stanza.
 */
remoting.ClientPlugin.prototype.onIncomingIq = function(iq) {};

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
 */
remoting.ClientPlugin.prototype.connect = function(
    hostJid, hostPublicKey, localJid, sharedSecret,
    authenticationMethods, authenticationTag) {};

/**
 * Release all currently pressed keys.
 */
remoting.ClientPlugin.prototype.releaseAllKeys = function() {};

/**
 * Send a key event to the host.
 *
 * @param {number} usbKeycode The USB-style code of the key to inject.
 * @param {boolean} pressed True to inject a key press, False for a release.
 */
remoting.ClientPlugin.prototype.injectKeyEvent =
    function(usbKeycode, pressed) {};

/**
 * Remap one USB keycode to another in all subsequent key events.
 *
 * @param {number} fromKeycode The USB-style code of the key to remap.
 * @param {number} toKeycode The USB-style code to remap the key to.
 */
remoting.ClientPlugin.prototype.remapKey =
    function(fromKeycode, toKeycode) {};

/**
 * Returns an associative array with a set of stats for this connection.
 *
 * @return {remoting.ClientSession.PerfStats} The connection statistics.
 */
remoting.ClientPlugin.prototype.getPerfStats = function() {};

/**
 * Sends a clipboard item to the host.
 *
 * @param {string} mimeType The MIME type of the clipboard item.
 * @param {string} item The clipboard item.
 */
remoting.ClientPlugin.prototype.sendClipboardItem = function(mimeType, item) {};

/**
 * Notifies the host that the client has the specified dimensions.
 *
 * @param {number} width The available client width.
 * @param {number} height The available client height.
 */
remoting.ClientPlugin.prototype.notifyClientDimensions =
    function(width, height) {};

/**
 * Requests that the host pause or resume sending video updates.
 *
 * @param {boolean} pause True to suspend video updates, false otherwise.
 */
remoting.ClientPlugin.prototype.pauseVideo =
    function(pause) {};
