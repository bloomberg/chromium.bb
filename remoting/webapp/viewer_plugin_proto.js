// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type definitions for the viewer plugin. It is used only
// with JSCompiler to verify the type-correctness of our code.

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor
 *  @extends HTMLEmbedElement
 */
remoting.ViewerPlugin = function() { };

/** @param {string} message The message to send to the host. */
remoting.ViewerPlugin.prototype.postMessage = function(message) {};

/** @param {string} iq The Iq stanza received from the host. */
remoting.ViewerPlugin.prototype.onIq = function(iq) {};

/** Release all keys currently pressed by this client. */
remoting.ViewerPlugin.prototype.releaseAllKeys = function() {};

/**
 * @param {string} hostJid The host's JID.
 * @param {string} hostPublicKey The host's public key.
 * @param {string} clientJid The client's JID.
 * @param {string} sharedSecret The access code for IT2Me or the
 *     PIN for Me2Me.
 * @param {string=} authenticationMethod Comma-separated list of
 *     authentication methods the client should attempt to use.
 * @param {string=} authenticationTag A host-specific tag to mix into
 *     authentication hashes.
 * @return {void} Nothing.
*/
remoting.ViewerPlugin.prototype.connect =
    function(hostJid, hostPublicKey, clientJid, sharedSecret,
             authenticationMethod, authenticationTag) {};

/**
 * @param {boolean} scaleToFit New scaleToFit value.
 */
remoting.ViewerPlugin.prototype.setScaleToFit = function(scaleToFit) {};

/** @type {function(number, number): void} State change callback function. */
remoting.ViewerPlugin.prototype.connectionInfoUpdate;

/** @type {number} */ remoting.ViewerPlugin.prototype.apiMinVersion;
/** @type {number} */ remoting.ViewerPlugin.prototype.apiVersion;

/** @type {number} */ remoting.ViewerPlugin.prototype.desktopHeight;
/** @type {number} */ remoting.ViewerPlugin.prototype.desktopWidth;

/** @type {number} */ remoting.ViewerPlugin.prototype.status;
/** @type {number} */ remoting.ViewerPlugin.prototype.error;

/** @type {number} */ remoting.ViewerPlugin.prototype.STATUS_UNKNOWN;
/** @type {number} */ remoting.ViewerPlugin.prototype.STATUS_CONNECTING;
/** @type {number} */ remoting.ViewerPlugin.prototype.STATUS_INITIALIZING;
/** @type {number} */ remoting.ViewerPlugin.prototype.STATUS_CONNECTED;
/** @type {number} */ remoting.ViewerPlugin.prototype.STATUS_CLOSED;
/** @type {number} */ remoting.ViewerPlugin.prototype.STATUS_FAILED;

/** @type {number} */ remoting.ViewerPlugin.prototype.ERROR_NONE;
/** @type {number} */ remoting.ViewerPlugin.prototype.ERROR_HOST_IS_OFFLINE;
/** @type {number} */ remoting.ViewerPlugin.prototype.ERROR_SESSION_REJECTED;
/** @type {number} */
    remoting.ViewerPlugin.prototype.ERROR_INCOMPATIBLE_PROTOCOL;
/** @type {number} */ remoting.ViewerPlugin.prototype.ERROR_NETWORK_FAILURE;

/** @type {number} */ remoting.ViewerPlugin.prototype.videoBandwidth;
/** @type {number} */ remoting.ViewerPlugin.prototype.videoCaptureLatency;
/** @type {number} */ remoting.ViewerPlugin.prototype.videoDecodeLatency;
/** @type {number} */ remoting.ViewerPlugin.prototype.videoEncodeLatency;
/** @type {number} */ remoting.ViewerPlugin.prototype.videoFrameRate;
/** @type {number} */ remoting.ViewerPlugin.prototype.videoRenderLatency;
/** @type {number} */ remoting.ViewerPlugin.prototype.roundTripLatency;
