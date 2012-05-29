// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class that wraps low-level details of interacting with the client plugin.
 *
 * This abstracts a <embed> element and controls the plugin which does
 * the actual remoting work. It also handles differences between
 * client plugins versions when it is necessary.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.ViewerPlugin} plugin The plugin embed element.
 * @constructor
 * @implements {remoting.ClientPlugin}
 */
remoting.ClientPluginV1 = function(plugin) {
  this.plugin = plugin;

  this.desktopWidth = 0;
  this.desktopHeight = 0;

  /** @param {string} iq The Iq stanza received from the host. */
  this.onOutgoingIqHandler = function (iq) {};
  /** @param {string} message Log message. */
  this.onDebugMessageHandler = function (message) {};
  /**
   * @param {number} status The plugin status.
   * @param {number} error The plugin error status, if any.
   */
  this.onConnectionStatusUpdateHandler = function(status, error) {};
  this.onDesktopSizeUpdateHandler = function () {};

  // Connect event handlers.

  /** @param {string} iq The IQ stanza to send. */
  this.plugin.sendIq = this.onSendIq_.bind(this);

  /** @param {string} msg The message to log. */
  this.plugin.debugInfo = this.onDebugInfo_.bind(this);

  /**
   * @param {number} status The plugin status.
   * @param {number} error The plugin error status, if any.
   */
  this.plugin.connectionInfoUpdate = this.onConnectionInfoUpdate_.bind(this);
  this.plugin.desktopSizeUpdate = this.onDesktopSizeUpdate_.bind(this);
};

/**
 * Chromoting session API version (for this javascript).
 * This is compared with the plugin API version to verify that they are
 * compatible.
 *
 * @const
 * @private
 */
remoting.ClientPluginV1.prototype.API_VERSION_ = 4;

/**
 * The oldest API version that we support.
 * This will differ from the |API_VERSION_| if we maintain backward
 * compatibility with older API versions.
 *
 * @const
 * @private
 */
remoting.ClientPluginV1.prototype.API_MIN_VERSION_ = 2;

/** @param {string} iq The Iq stanza received from the host. */
remoting.ClientPluginV1.prototype.onSendIq_ = function(iq) {
  this.onOutgoingIqHandler(iq);
}

  /** @param {string} message The IQ stanza to send. */
remoting.ClientPluginV1.prototype.onDebugInfo_ = function(message) {
  this.onDebugMessageHandler(message);
}

/**
 * @param {number} status The plugin status.
 * @param {number} error The plugin error status, if any.
 */
remoting.ClientPluginV1.prototype.onConnectionInfoUpdate_=
    function(status, error) {
  // Old plugins didn't pass the status and error values, so get
  // them directly. Note that there is a race condition inherent in
  // this approach.
  if (typeof(status) == 'undefined')
    status = this.plugin.status;
  if (typeof(error) == 'undefined')
    error = this.plugin.error;
  this.onConnectionStatusUpdateHandler(status, error);
};

remoting.ClientPluginV1.prototype.onDesktopSizeUpdate_ = function() {
  this.desktopWidth = this.plugin.desktopWidth;
  this.desktopHeight = this.plugin.desktopHeight;
  this.onDesktopSizeUpdateHandler();
}

/**
 * Deletes the plugin.
 */
remoting.ClientPluginV1.prototype.cleanup = function() {
  this.plugin.parentNode.removeChild(this.plugin);
};

/**
 * @return {HTMLEmbedElement} HTML element that correspods to the plugin.
 */
remoting.ClientPluginV1.prototype.element = function() {
  return this.plugin;
};

/**
 * @param {function(boolean): void} onDone
 */
remoting.ClientPluginV1.prototype.initialize = function(onDone) {
  onDone(typeof this.plugin.connect === 'function');
};

/**
 * @return {boolean} True if the plugin and web-app versions are compatible.
 */
remoting.ClientPluginV1.prototype.isSupportedVersion = function() {
  return this.API_VERSION_ >= this.plugin.apiMinVersion &&
      this.plugin.apiVersion >= this.API_MIN_VERSION_;
};

/**
 * @param {remoting.ClientPlugin.Feature} feature The feature to test for.
 * @return {boolean} True if the plugin supports the named feature.
 */
remoting.ClientPluginV1.prototype.hasFeature = function(feature) {
  if (feature == remoting.ClientPlugin.Feature.HIGH_QUALITY_SCALING)
    return this.plugin.apiVersion >= 3;
  return false;
};

/**
 * @return {boolean} True if the plugin supports the injectKeyEvent API.
 */
remoting.ClientPluginV1.prototype.isInjectKeyEventSupported = function() {
  return false;
};

/**
 * @param {string} iq Incoming IQ stanza.
 */
remoting.ClientPluginV1.prototype.onIncomingIq = function(iq) {
  if (this.plugin && this.plugin.onIq) {
    this.plugin.onIq(iq);
  } else {
    // plugin.onIq may not be set after the plugin has been shut
    // down. Particularly this happens when we receive response to
    // session-terminate stanza.
    console.warn('plugin.onIq is not set so dropping incoming message.');
  }
};

/**
 * @param {string} hostJid The jid of the host to connect to.
 * @param {string} hostPublicKey The base64 encoded version of the host's
 *     public key.
 * @param {string} clientJid Local jid.
 * @param {string} sharedSecret The access code for IT2Me or the PIN
 *     for Me2Me.
 * @param {string} authenticationMethods Comma-separated list of
 *     authentication methods the client should attempt to use.
 * @param {string} authenticationTag A host-specific tag to mix into
 *     authentication hashes.
 */
remoting.ClientPluginV1.prototype.connect = function(
    hostJid, hostPublicKey, clientJid, sharedSecret,
    authenticationMethods, authenticationTag) {
  if (this.plugin.apiVersion < 4) {
    // Client plugin versions prior to 4 didn't support the last two
    // parameters.
    this.plugin.connect(hostJid, hostPublicKey, clientJid, sharedSecret);
  } else {
    this.plugin.connect(hostJid, hostPublicKey, clientJid, sharedSecret,
                        authenticationMethods, authenticationTag);
  }
};

/**
 * @param {boolean} scaleToFit True if scale-to-fit should be enabled.
 */
remoting.ClientPluginV1.prototype.setScaleToFit = function(scaleToFit) {
  // scaleToFit() will be removed in future versions of the plugin.
  if (!!this.plugin && typeof this.plugin.setScaleToFit === 'function')
    this.plugin.setScaleToFit(scaleToFit);
};


/**
 * Release all currently pressed keys.
 */
remoting.ClientPluginV1.prototype.releaseAllKeys = function() {
  this.plugin.releaseAllKeys();
};

/**
 * Returns an associative array with a set of stats for this connection.
 *
 * @return {remoting.ClientSession.PerfStats} The connection statistics.
 */
remoting.ClientPluginV1.prototype.getPerfStats = function() {
  /** @type {remoting.ClientSession.PerfStats} */
  return { videoBandwidth: this.plugin.videoBandwidth,
           videoFrameRate: this.plugin.videoFrameRate,
           captureLatency: this.plugin.videoCaptureLatency,
           encodeLatency: this.plugin.videoEncodeLatency,
           decodeLatency: this.plugin.videoDecodeLatency,
           renderLatency: this.plugin.videoRenderLatency,
           roundtripLatency: this.plugin.roundTripLatency };
};

/**
 * These dummy methods exist only so that this class implements ClientPlugin.
 */

/**
 * @param {string} mimeType
 * @param {string} item
 */
remoting.ClientPluginV1.prototype.sendClipboardItem = function(mimeType, item) {
  return;
};

/**
 * @param {number} usbKeycode
 * @param {boolean} pressed
 */
remoting.ClientPluginV1.prototype.injectKeyEvent =
    function(usbKeycode, pressed) {
  return;
};

/**
 * @param {number} fromKeycode
 * @param {number} toKeycode
 */
remoting.ClientPluginV1.prototype.remapKey =
    function(fromKeycode, toKeycode) {
  return;
};

/**
 * @param {number} width
 * @param {number} height
 */
remoting.ClientPluginV1.prototype.notifyClientDimensions =
    function(width, height) {
  return;
};

/**
 * @param {boolean} pause
 */
remoting.ClientPluginV1.prototype.pauseVideo =
    function(pause) {
  return;
};
