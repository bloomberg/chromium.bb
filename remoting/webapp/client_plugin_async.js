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
remoting.ClientPluginAsync = function(plugin) {
  this.plugin = plugin;

  this.desktopWidth = 0;
  this.desktopHeight = 0;

  /** @param {string} iq The Iq stanza received from the host. */
  this.onOutgoingIqHandler = function (iq) {};
  /** @param {string} message Log message. */
  this.onDebugMessageHandler = function (message) {};
  /**
   * @param {number} state The connection state.
   * @param {number} error The error code, if any.
   */
  this.onConnectionStatusUpdateHandler = function(state, error) {};
  this.onDesktopSizeUpdateHandler = function () {};

  /** @type {number} */
  this.pluginApiVersion_ = -1;
  /** @type {number} */
  this.pluginApiMinVersion_ = -1;
  /** @type {boolean} */
  this.helloReceived_ = false;
  /** @type {function(boolean)|null} */
  this.onInitializedCallback_ = null;

  /** @type {remoting.ClientSession.PerfStats} */
  this.perfStats_ = new remoting.ClientSession.PerfStats();

  /** @type {remoting.ClientPluginAsync} */
  var that = this;
  /** @param {Event} event Message event from the plugin. */
  this.plugin.addEventListener('message', function(event) {
      that.handleMessage_(event.data);
    }, false);
};

/**
 * Chromoting session API version (for this javascript).
 * This is compared with the plugin API version to verify that they are
 * compatible.
 *
 * @const
 * @private
 */
remoting.ClientPluginAsync.prototype.API_VERSION_ = 5;

/**
 * The oldest API version that we support.
 * This will differ from the |API_VERSION_| if we maintain backward
 * compatibility with older API versions.
 *
 * @const
 * @private
 */
remoting.ClientPluginAsync.prototype.API_MIN_VERSION_ = 5;

/**
 * @param {string} message_str Message from the plugin.
 */
remoting.ClientPluginAsync.prototype.handleMessage_ = function(message_str) {
  var message = /** @type {{method:string, data:Object.<string,string>}} */
      JSON.parse(message_str);

  if (!('method' in message) || !('data' in message)) {
    console.error('Received invalid message from the plugin: ' + message_str);
    return;
  }

  if (message.method == 'hello') {
    if (typeof message.data['apiVersion'] != 'number' ||
        typeof message.data['apiMinVersion'] != 'number') {
      console.error('Received invalid hello message: ' + message_str);
      return;
    }
    this.pluginApiVersion_ = /** @type {number} */ message.data['apiVersion'];
    this.pluginApiMinVersion_ =
        /** @type {number} */ message.data['apiMinVersion'];
    this.helloReceived_ = true;
    if (this.onInitializedCallback_ != null) {
      this.onInitializedCallback_(true);
      this.onInitializedCallback_ = null;
    }
  } else if (message.method == 'sendOutgoingIq') {
    if (typeof message.data['iq'] != 'string') {
      console.error('Received invalid sendOutgoingIq message: ' + message_str);
      return;
    }
    this.onOutgoingIqHandler(message.data['iq']);
  } else if (message.method == 'logDebugMessage') {
    if (typeof message.data['message'] != 'string') {
      console.error('Received invalid logDebugMessage message: ' + message_str);
      return;
    }
    this.onDebugMessageHandler(message.data['message']);
  } else if (message.method == 'onConnectionStatus') {
    if (typeof message.data['state'] != 'string' ||
        !(message.data['state'] in remoting.ClientSession.State) ||
        typeof message.data['error'] != 'string') {
      console.error('Received invalid onConnectionState message: ' +
                    message_str);
      return;
    }

    /** @type {remoting.ClientSession.State} */
    var state = remoting.ClientSession.State[message.data['state']];
    var error;
    if (message.data['error'] in remoting.ClientSession.ConnectionError) {
      error = /** @type {remoting.ClientSession.ConnectionError} */
          remoting.ClientSession.ConnectionError[message.data['error']];
    } else {
      error = remoting.ClientSession.ConnectionError.UNKNOWN;
    }

    this.onConnectionStatusUpdateHandler(state, error);
  } else if (message.method == 'onDesktopSize') {
    if (typeof message.data['width'] != 'number' ||
        typeof message.data['height'] != 'number') {
      console.error('Received invalid onDesktopSize message: ' + message_str);
      return;
    }
    this.desktopWidth = /** @type {number} */ message.data['width'];
    this.desktopHeight = /** @type {number} */ message.data['height'];
    this.onDesktopSizeUpdateHandler();
  } else if (message.method == 'onPerfStats') {
    if (typeof message.data['videoBandwidth'] != 'number' ||
        typeof message.data['videoFrameRate'] != 'number' ||
        typeof message.data['captureLatency'] != 'number' ||
        typeof message.data['encodeLatency'] != 'number' ||
        typeof message.data['decodeLatency'] != 'number' ||
        typeof message.data['renderLatency'] != 'number' ||
        typeof message.data['roundtripLatency'] != 'number') {
      console.error('Received incorrect onPerfStats message: ' + message_str);
      return;
    }
    this.perfStats_ =
        /** @type {remoting.ClientSession.PerfStats} */ message.data;
  }
}

/**
 * Deletes the plugin.
 */
remoting.ClientPluginAsync.prototype.cleanup = function() {
  this.plugin.parentNode.removeChild(this.plugin);
};

/**
 * @return {Element} HTML element that correspods to the plugin.
 */
remoting.ClientPluginAsync.prototype.element = function() {
  return this.plugin;
};

/**
 * @param {function(boolean): void} onDone
 */
remoting.ClientPluginAsync.prototype.initialize = function(onDone) {
  if (this.helloReceived_) {
    onDone(true);
  } else {
    this.onInitializedCallback_ = onDone;
  }
};

/**
 * @return {boolean} True if the plugin and web-app versions are compatible.
 */
remoting.ClientPluginAsync.prototype.isSupportedVersion = function() {
  if (!this.helloReceived_) {
    console.error(
        "isSupportedVersion() is called before the plugin is initialized.");
    return false;
  }
  return this.API_VERSION_ >= this.pluginApiMinVersion_ &&
      this.pluginApiVersion_ >= this.API_MIN_VERSION_;
};

/**
 * @return {boolean} True if the plugin supports high-quality scaling.
 */
remoting.ClientPluginAsync.prototype.isHiQualityScalingSupported = function() {
  return true;
};

/**
 * @param {string} iq Incoming IQ stanza.
 */
remoting.ClientPluginAsync.prototype.onIncomingIq = function(iq) {
  if (this.plugin && this.plugin.postMessage) {
    this.plugin.postMessage(JSON.stringify(
        { method: 'incomingIq', data: { iq: iq } }));
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
 * @param {string} localJid Local jid.
 * @param {string} sharedSecret The access code for IT2Me or the PIN
 *     for Me2Me.
 * @param {string} authenticationMethods Comma-separated list of
 *     authentication methods the client should attempt to use.
 * @param {string} authenticationTag A host-specific tag to mix into
 *     authentication hashes.
 */
remoting.ClientPluginAsync.prototype.connect = function(
    hostJid, hostPublicKey, localJid, sharedSecret,
    authenticationMethods, authenticationTag) {
  this.plugin.postMessage(JSON.stringify(
    { method: 'connect', data: {
        hostJid: hostJid,
        hostPublicKey: hostPublicKey,
        localJid: localJid,
        sharedSecret: sharedSecret,
        authenticationMethods: authenticationMethods,
        authenticationTag: authenticationTag
      }
    }));
};

/**
 * @param {boolean} scaleToFit True if scale-to-fit should be enabled.
 */
remoting.ClientPluginAsync.prototype.setScaleToFit = function(scaleToFit) {
  // scaleToFit() will be removed in future versions of the plugin.
  if (this.plugin && typeof this.plugin.setScaleToFit === 'function')
    this.plugin.setScaleToFit(scaleToFit);
};


/**
 * Release all currently pressed keys.
 */
remoting.ClientPluginAsync.prototype.releaseAllKeys = function() {
  this.plugin.postMessage(JSON.stringify(
      { method: 'releaseAllKeys', data: {} }));
};

/**
 * Returns an associative array with a set of stats for this connecton.
 *
 * @return {remoting.ClientSession.PerfStats} The connection statistics.
 */
remoting.ClientPluginAsync.prototype.getPerfStats = function() {
  return this.perfStats_;
};
