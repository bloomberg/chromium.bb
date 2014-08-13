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
 * @param {Element} container The container for the embed element.
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @constructor
 */
remoting.ClientPlugin = function(container, onExtensionMessage) {
  this.plugin_ = /** @type {remoting.ViewerPlugin} */
      document.createElement('embed');

  this.plugin_.id = 'session-client-plugin';
  if (remoting.settings.CLIENT_PLUGIN_TYPE == 'pnacl') {
    this.plugin_.src = 'remoting_client_pnacl.nmf';
    this.plugin_.type = 'application/x-pnacl';
  } else if (remoting.settings.CLIENT_PLUGIN_TYPE == 'nacl') {
    this.plugin_.src = 'remoting_client_nacl.nmf';
    this.plugin_.type = 'application/x-nacl';
  } else {
    this.plugin_.src = 'about://none';
    this.plugin_.type = 'application/vnd.chromium.remoting-viewer';
  }

  this.plugin_.width = 0;
  this.plugin_.height = 0;
  this.plugin_.tabIndex = 0;  // Required, otherwise focus() doesn't work.
  container.appendChild(this.plugin_);

  this.onExtensionMessage_ = onExtensionMessage;

  this.desktopWidth = 0;
  this.desktopHeight = 0;
  this.desktopXDpi = 96;
  this.desktopYDpi = 96;

  /** @param {string} iq The Iq stanza received from the host. */
  this.onOutgoingIqHandler = function (iq) {};
  /** @param {string} message Log message. */
  this.onDebugMessageHandler = function (message) {};
  /**
   * @param {number} state The connection state.
   * @param {number} error The error code, if any.
   */
  this.onConnectionStatusUpdateHandler = function(state, error) {};
  /** @param {boolean} ready Connection ready state. */
  this.onConnectionReadyHandler = function(ready) {};

  /**
   * @param {string} tokenUrl Token-request URL, received from the host.
   * @param {string} hostPublicKey Public key for the host.
   * @param {string} scope OAuth scope to request the token for.
   */
  this.fetchThirdPartyTokenHandler = function(
    tokenUrl, hostPublicKey, scope) {};
  this.onDesktopSizeUpdateHandler = function () {};
  /** @param {!Array.<string>} capabilities The negotiated capabilities. */
  this.onSetCapabilitiesHandler = function (capabilities) {};
  this.fetchPinHandler = function (supportsPairing) {};
  /** @param {string} data Remote gnubbyd data. */
  this.onGnubbyAuthHandler = function(data) {};
  /**
   * @param {string} url
   * @param {number} hotspotX
   * @param {number} hotspotY
   */
  this.updateMouseCursorImage = function(url, hotspotX, hotspotY) {};

  /** @type {remoting.MediaSourceRenderer} */
  this.mediaSourceRenderer_ = null;

  /** @type {number} */
  this.pluginApiVersion_ = -1;
  /** @type {Array.<string>} */
  this.pluginApiFeatures_ = [];
  /** @type {number} */
  this.pluginApiMinVersion_ = -1;
  /** @type {!Array.<string>} */
  this.capabilities_ = [];
  /** @type {boolean} */
  this.helloReceived_ = false;
  /** @type {function(boolean)|null} */
  this.onInitializedCallback_ = null;
  /** @type {function(string, string):void} */
  this.onPairingComplete_ = function(clientId, sharedSecret) {};
  /** @type {remoting.ClientSession.PerfStats} */
  this.perfStats_ = new remoting.ClientSession.PerfStats();

  /** @type {remoting.ClientPlugin} */
  var that = this;
  /** @param {Event} event Message event from the plugin. */
  this.plugin_.addEventListener('message', function(event) {
      that.handleMessage_(event.data);
    }, false);

  if (remoting.settings.CLIENT_PLUGIN_TYPE == 'native') {
    window.setTimeout(this.showPluginForClickToPlay_.bind(this), 500);
  }
};

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
 * Chromoting session API version (for this javascript).
 * This is compared with the plugin API version to verify that they are
 * compatible.
 *
 * @const
 * @private
 */
remoting.ClientPlugin.prototype.API_VERSION_ = 6;

/**
 * The oldest API version that we support.
 * This will differ from the |API_VERSION_| if we maintain backward
 * compatibility with older API versions.
 *
 * @const
 * @private
 */
remoting.ClientPlugin.prototype.API_MIN_VERSION_ = 5;

/**
 * @param {string|{method:string, data:Object.<string,*>}}
 *    rawMessage Message from the plugin.
 * @private
 */
remoting.ClientPlugin.prototype.handleMessage_ = function(rawMessage) {
  var message =
      /** @type {{method:string, data:Object.<string,*>}} */
      ((typeof(rawMessage) == 'string') ? jsonParseSafe(rawMessage)
                                        : rawMessage);
  if (!message || !('method' in message) || !('data' in message)) {
    console.error('Received invalid message from the plugin:', rawMessage);
    return;
  }

  try {
    this.handleMessageMethod_(message);
  } catch(e) {
    console.error(/** @type {*} */ (e));
  }
}

/**
 * @param {{method:string, data:Object.<string,*>}}
 *    message Parsed message from the plugin.
 * @private
 */
remoting.ClientPlugin.prototype.handleMessageMethod_ = function(message) {
  /**
   * Splits a string into a list of words delimited by spaces.
   * @param {string} str String that should be split.
   * @return {!Array.<string>} List of words.
   */
  var tokenize = function(str) {
    /** @type {Array.<string>} */
    var tokens = str.match(/\S+/g);
    return tokens ? tokens : [];
  };

  if (message.method == 'hello') {
    // Resize in case we had to enlarge it to support click-to-play.
    this.hidePluginForClickToPlay_();
    this.pluginApiVersion_ = getNumberAttr(message.data, 'apiVersion');
    this.pluginApiMinVersion_ = getNumberAttr(message.data, 'apiMinVersion');

    if (this.pluginApiVersion_ >= 7) {
      this.pluginApiFeatures_ =
          tokenize(getStringAttr(message.data, 'apiFeatures'));

      // Negotiate capabilities.

      /** @type {!Array.<string>} */
      var requestedCapabilities = [];
      if ('requestedCapabilities' in message.data) {
        requestedCapabilities =
            tokenize(getStringAttr(message.data, 'requestedCapabilities'));
      }

      /** @type {!Array.<string>} */
      var supportedCapabilities = [];
      if ('supportedCapabilities' in message.data) {
        supportedCapabilities =
            tokenize(getStringAttr(message.data, 'supportedCapabilities'));
      }

      // At the moment the webapp does not recognize any of
      // 'requestedCapabilities' capabilities (so they all should be disabled)
      // and do not care about any of 'supportedCapabilities' capabilities (so
      // they all can be enabled).
      this.capabilities_ = supportedCapabilities;

      // Let the host know that the webapp can be requested to always send
      // the client's dimensions.
      this.capabilities_.push(
          remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION);

      // Let the host know that we're interested in knowing whether or not
      // it rate-limits desktop-resize requests.
      this.capabilities_.push(
          remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS);

      // Let the host know that we can use the video framerecording extension.
      this.capabilities_.push(
          remoting.ClientSession.Capability.VIDEO_RECORDER);
    } else if (this.pluginApiVersion_ >= 6) {
      this.pluginApiFeatures_ = ['highQualityScaling', 'injectKeyEvent'];
    } else {
      this.pluginApiFeatures_ = ['highQualityScaling'];
    }
    this.helloReceived_ = true;
    if (this.onInitializedCallback_ != null) {
      this.onInitializedCallback_(true);
      this.onInitializedCallback_ = null;
    }

  } else if (message.method == 'sendOutgoingIq') {
    this.onOutgoingIqHandler(getStringAttr(message.data, 'iq'));

  } else if (message.method == 'logDebugMessage') {
    this.onDebugMessageHandler(getStringAttr(message.data, 'message'));

  } else if (message.method == 'onConnectionStatus') {
    var state = remoting.ClientSession.State.fromString(
        getStringAttr(message.data, 'state'))
    var error = remoting.ClientSession.ConnectionError.fromString(
        getStringAttr(message.data, 'error'));
    this.onConnectionStatusUpdateHandler(state, error);

  } else if (message.method == 'onDesktopSize') {
    this.desktopWidth = getNumberAttr(message.data, 'width');
    this.desktopHeight = getNumberAttr(message.data, 'height');
    this.desktopXDpi = getNumberAttr(message.data, 'x_dpi', 96);
    this.desktopYDpi = getNumberAttr(message.data, 'y_dpi', 96);
    this.onDesktopSizeUpdateHandler();

  } else if (message.method == 'onPerfStats') {
    // Return value is ignored. These calls will throw an error if the value
    // is not a number.
    getNumberAttr(message.data, 'videoBandwidth');
    getNumberAttr(message.data, 'videoFrameRate');
    getNumberAttr(message.data, 'captureLatency');
    getNumberAttr(message.data, 'encodeLatency');
    getNumberAttr(message.data, 'decodeLatency');
    getNumberAttr(message.data, 'renderLatency');
    getNumberAttr(message.data, 'roundtripLatency');
    this.perfStats_ =
        /** @type {remoting.ClientSession.PerfStats} */ message.data;

  } else if (message.method == 'injectClipboardItem') {
    var mimetype = getStringAttr(message.data, 'mimeType');
    var item = getStringAttr(message.data, 'item');
    if (remoting.clipboard) {
      remoting.clipboard.fromHost(mimetype, item);
    }

  } else if (message.method == 'onFirstFrameReceived') {
    if (remoting.clientSession) {
      remoting.clientSession.onFirstFrameReceived();
    }

  } else if (message.method == 'onConnectionReady') {
    var ready = getBooleanAttr(message.data, 'ready');
    this.onConnectionReadyHandler(ready);

  } else if (message.method == 'fetchPin') {
    // The pairingSupported value in the dictionary indicates whether both
    // client and host support pairing. If the client doesn't support pairing,
    // then the value won't be there at all, so give it a default of false.
    var pairingSupported = getBooleanAttr(message.data, 'pairingSupported',
                                          false)
    this.fetchPinHandler(pairingSupported);

  } else if (message.method == 'setCapabilities') {
    /** @type {!Array.<string>} */
    var capabilities = tokenize(getStringAttr(message.data, 'capabilities'));
    this.onSetCapabilitiesHandler(capabilities);

  } else if (message.method == 'fetchThirdPartyToken') {
    var tokenUrl = getStringAttr(message.data, 'tokenUrl');
    var hostPublicKey = getStringAttr(message.data, 'hostPublicKey');
    var scope = getStringAttr(message.data, 'scope');
    this.fetchThirdPartyTokenHandler(tokenUrl, hostPublicKey, scope);

  } else if (message.method == 'pairingResponse') {
    var clientId = getStringAttr(message.data, 'clientId');
    var sharedSecret = getStringAttr(message.data, 'sharedSecret');
    this.onPairingComplete_(clientId, sharedSecret);

  } else if (message.method == 'extensionMessage') {
    var extMsgType = getStringAttr(message.data, 'type');
    var extMsgData = getStringAttr(message.data, 'data');
    switch (extMsgType) {
      case 'gnubby-auth':
        this.onGnubbyAuthHandler(extMsgData);
        break;
      case 'test-echo-reply':
        console.log('Got echo reply: ' + extMsgData);
        break;
      default:
        if (!this.onExtensionMessage_(extMsgType, extMsgData)) {
          console.log('Unexpected message received: ' +
                      extMsgType + ': ' + extMsgData);
        }
    }

  } else if (message.method == 'mediaSourceReset') {
    if (!this.mediaSourceRenderer_) {
      console.error('Unexpected mediaSourceReset.');
      return;
    }
    this.mediaSourceRenderer_.reset(getStringAttr(message.data, 'format'))

  } else if (message.method == 'mediaSourceData') {
    if (!(message.data['buffer'] instanceof ArrayBuffer)) {
      console.error('Invalid mediaSourceData message:', message.data);
      return;
    }
    if (!this.mediaSourceRenderer_) {
      console.error('Unexpected mediaSourceData.');
      return;
    }
    // keyframe flag may be absent from the message.
    var keyframe = !!message.data['keyframe'];
    this.mediaSourceRenderer_.onIncomingData(
        (/** @type {ArrayBuffer} */ message.data['buffer']), keyframe);

  } else if (message.method == 'unsetCursorShape') {
    this.updateMouseCursorImage('', 0, 0);

  } else if (message.method == 'setCursorShape') {
    var width = getNumberAttr(message.data, 'width');
    var height = getNumberAttr(message.data, 'height');
    var hotspotX = getNumberAttr(message.data, 'hotspotX');
    var hotspotY = getNumberAttr(message.data, 'hotspotY');
    var srcArrayBuffer = getObjectAttr(message.data, 'data');

    var canvas =
        /** @type {HTMLCanvasElement} */ (document.createElement('canvas'));
    canvas.width = width;
    canvas.height = height;

    var context =
        /** @type {CanvasRenderingContext2D} */ (canvas.getContext('2d'));
    var imageData = context.getImageData(0, 0, width, height);
    base.debug.assert(srcArrayBuffer instanceof ArrayBuffer);
    var src = new Uint8Array(/** @type {ArrayBuffer} */(srcArrayBuffer));
    var dest = imageData.data;
    for (var i = 0; i < /** @type {number} */(dest.length); i += 4) {
      dest[i] = src[i + 2];
      dest[i + 1] = src[i + 1];
      dest[i + 2] = src[i];
      dest[i + 3] = src[i + 3];
    }

    context.putImageData(imageData, 0, 0);
    this.updateMouseCursorImage(canvas.toDataURL(), hotspotX, hotspotY);
  }
};

/**
 * Deletes the plugin.
 */
remoting.ClientPlugin.prototype.cleanup = function() {
  if (this.plugin_) {
    this.plugin_.parentNode.removeChild(this.plugin_);
    this.plugin_ = null;
  }
};

/**
 * @return {HTMLEmbedElement} HTML element that corresponds to the plugin.
 */
remoting.ClientPlugin.prototype.element = function() {
  return this.plugin_;
};

/**
 * @param {function(boolean): void} onDone
 */
remoting.ClientPlugin.prototype.initialize = function(onDone) {
  if (this.helloReceived_) {
    onDone(true);
  } else {
    this.onInitializedCallback_ = onDone;
  }
};

/**
 * @return {boolean} True if the plugin and web-app versions are compatible.
 */
remoting.ClientPlugin.prototype.isSupportedVersion = function() {
  if (!this.helloReceived_) {
    console.error(
        "isSupportedVersion() is called before the plugin is initialized.");
    return false;
  }
  return this.API_VERSION_ >= this.pluginApiMinVersion_ &&
      this.pluginApiVersion_ >= this.API_MIN_VERSION_;
};

/**
 * @param {remoting.ClientPlugin.Feature} feature The feature to test for.
 * @return {boolean} True if the plugin supports the named feature.
 */
remoting.ClientPlugin.prototype.hasFeature = function(feature) {
  if (!this.helloReceived_) {
    console.error(
        "hasFeature() is called before the plugin is initialized.");
    return false;
  }
  return this.pluginApiFeatures_.indexOf(feature) > -1;
};

/**
 * @return {boolean} True if the plugin supports the injectKeyEvent API.
 */
remoting.ClientPlugin.prototype.isInjectKeyEventSupported = function() {
  return this.pluginApiVersion_ >= 6;
};

/**
 * @param {string} iq Incoming IQ stanza.
 */
remoting.ClientPlugin.prototype.onIncomingIq = function(iq) {
  if (this.plugin_ && this.plugin_.postMessage) {
    this.plugin_.postMessage(JSON.stringify(
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
 * @param {string} clientPairingId For paired Me2Me connections, the
 *     pairing id for this client, as issued by the host.
 * @param {string} clientPairedSecret For paired Me2Me connections, the
 *     paired secret for this client, as issued by the host.
 */
remoting.ClientPlugin.prototype.connect = function(
    hostJid, hostPublicKey, localJid, sharedSecret,
    authenticationMethods, authenticationTag,
    clientPairingId, clientPairedSecret) {
  var keyFilter = '';
  if (remoting.platformIsMac()) {
    keyFilter = 'mac';
  } else if (remoting.platformIsChromeOS()) {
    keyFilter = 'cros';
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'delegateLargeCursors', data: {} }));
  this.plugin_.postMessage(JSON.stringify(
    { method: 'connect', data: {
        hostJid: hostJid,
        hostPublicKey: hostPublicKey,
        localJid: localJid,
        sharedSecret: sharedSecret,
        authenticationMethods: authenticationMethods,
        authenticationTag: authenticationTag,
        capabilities: this.capabilities_.join(" "),
        clientPairingId: clientPairingId,
        clientPairedSecret: clientPairedSecret,
        keyFilter: keyFilter
      }
    }));
};

/**
 * Release all currently pressed keys.
 */
remoting.ClientPlugin.prototype.releaseAllKeys = function() {
  this.plugin_.postMessage(JSON.stringify(
      { method: 'releaseAllKeys', data: {} }));
};

/**
 * Send a key event to the host.
 *
 * @param {number} usbKeycode The USB-style code of the key to inject.
 * @param {boolean} pressed True to inject a key press, False for a release.
 */
remoting.ClientPlugin.prototype.injectKeyEvent =
    function(usbKeycode, pressed) {
  this.plugin_.postMessage(JSON.stringify(
      { method: 'injectKeyEvent', data: {
          'usbKeycode': usbKeycode,
          'pressed': pressed}
      }));
};

/**
 * Remap one USB keycode to another in all subsequent key events.
 *
 * @param {number} fromKeycode The USB-style code of the key to remap.
 * @param {number} toKeycode The USB-style code to remap the key to.
 */
remoting.ClientPlugin.prototype.remapKey =
    function(fromKeycode, toKeycode) {
  this.plugin_.postMessage(JSON.stringify(
      { method: 'remapKey', data: {
          'fromKeycode': fromKeycode,
          'toKeycode': toKeycode}
      }));
};

/**
 * Enable/disable redirection of the specified key to the web-app.
 *
 * @param {number} keycode The USB-style code of the key.
 * @param {Boolean} trap True to enable trapping, False to disable.
 */
remoting.ClientPlugin.prototype.trapKey = function(keycode, trap) {
  this.plugin_.postMessage(JSON.stringify(
      { method: 'trapKey', data: {
          'keycode': keycode,
          'trap': trap}
      }));
};

/**
 * Returns an associative array with a set of stats for this connecton.
 *
 * @return {remoting.ClientSession.PerfStats} The connection statistics.
 */
remoting.ClientPlugin.prototype.getPerfStats = function() {
  return this.perfStats_;
};

/**
 * Sends a clipboard item to the host.
 *
 * @param {string} mimeType The MIME type of the clipboard item.
 * @param {string} item The clipboard item.
 */
remoting.ClientPlugin.prototype.sendClipboardItem =
    function(mimeType, item) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.SEND_CLIPBOARD_ITEM))
    return;
  this.plugin_.postMessage(JSON.stringify(
      { method: 'sendClipboardItem',
        data: { mimeType: mimeType, item: item }}));
};

/**
 * Notifies the host that the client has the specified size and pixel density.
 *
 * @param {number} width The available client width in DIPs.
 * @param {number} height The available client height in DIPs.
 * @param {number} device_scale The number of device pixels per DIP.
 */
remoting.ClientPlugin.prototype.notifyClientResolution =
    function(width, height, device_scale) {
  if (this.hasFeature(remoting.ClientPlugin.Feature.NOTIFY_CLIENT_RESOLUTION)) {
    var dpi = Math.floor(device_scale * 96);
    this.plugin_.postMessage(JSON.stringify(
        { method: 'notifyClientResolution',
          data: { width: Math.floor(width * device_scale),
                  height: Math.floor(height * device_scale),
                  x_dpi: dpi, y_dpi: dpi }}));
  }
};

/**
 * Requests that the host pause or resume sending video updates.
 *
 * @param {boolean} pause True to suspend video updates, false otherwise.
 */
remoting.ClientPlugin.prototype.pauseVideo =
    function(pause) {
  if (this.hasFeature(remoting.ClientPlugin.Feature.VIDEO_CONTROL)) {
    this.plugin_.postMessage(JSON.stringify(
        { method: 'videoControl', data: { pause: pause }}));
  } else if (this.hasFeature(remoting.ClientPlugin.Feature.PAUSE_VIDEO)) {
    this.plugin_.postMessage(JSON.stringify(
        { method: 'pauseVideo', data: { pause: pause }}));
  }
};

/**
 * Requests that the host pause or resume sending audio updates.
 *
 * @param {boolean} pause True to suspend audio updates, false otherwise.
 */
remoting.ClientPlugin.prototype.pauseAudio =
    function(pause) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.PAUSE_AUDIO)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'pauseAudio', data: { pause: pause }}));
};

/**
 * Requests that the host configure the video codec for lossless encode.
 *
 * @param {boolean} wantLossless True to request lossless encoding.
 */
remoting.ClientPlugin.prototype.setLosslessEncode =
    function(wantLossless) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.VIDEO_CONTROL)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'videoControl', data: { losslessEncode: wantLossless }}));
};

/**
 * Requests that the host configure the video codec for lossless color.
 *
 * @param {boolean} wantLossless True to request lossless color.
 */
remoting.ClientPlugin.prototype.setLosslessColor =
    function(wantLossless) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.VIDEO_CONTROL)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'videoControl', data: { losslessColor: wantLossless }}));
};

/**
 * Called when a PIN is obtained from the user.
 *
 * @param {string} pin The PIN.
 */
remoting.ClientPlugin.prototype.onPinFetched =
    function(pin) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.ASYNC_PIN)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'onPinFetched', data: { pin: pin }}));
};

/**
 * Tells the plugin to ask for the PIN asynchronously.
 */
remoting.ClientPlugin.prototype.useAsyncPinDialog =
    function() {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.ASYNC_PIN)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'useAsyncPinDialog', data: {} }));
};

/**
 * Sets the third party authentication token and shared secret.
 *
 * @param {string} token The token received from the token URL.
 * @param {string} sharedSecret Shared secret received from the token URL.
 */
remoting.ClientPlugin.prototype.onThirdPartyTokenFetched = function(
    token, sharedSecret) {
  this.plugin_.postMessage(JSON.stringify(
    { method: 'onThirdPartyTokenFetched',
      data: { token: token, sharedSecret: sharedSecret}}));
};

/**
 * Request pairing with the host for PIN-less authentication.
 *
 * @param {string} clientName The human-readable name of the client.
 * @param {function(string, string):void} onDone, Callback to receive the
 *     client id and shared secret when they are available.
 */
remoting.ClientPlugin.prototype.requestPairing =
    function(clientName, onDone) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.PINLESS_AUTH)) {
    return;
  }
  this.onPairingComplete_ = onDone;
  this.plugin_.postMessage(JSON.stringify(
      { method: 'requestPairing', data: { clientName: clientName } }));
};

/**
 * Send an extension message to the host.
 *
 * @param {string} type The message type.
 * @param {string} message The message payload.
 */
remoting.ClientPlugin.prototype.sendClientMessage =
    function(type, message) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.EXTENSION_MESSAGE)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'extensionMessage',
        data: { type: type, data: message } }));

};

/**
 * Request MediaStream-based rendering.
 *
 * @param {remoting.MediaSourceRenderer} mediaSourceRenderer
 */
remoting.ClientPlugin.prototype.enableMediaSourceRendering =
    function(mediaSourceRenderer) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.MEDIA_SOURCE_RENDERING)) {
    return;
  }
  this.mediaSourceRenderer_ = mediaSourceRenderer;
  this.plugin_.postMessage(JSON.stringify(
      { method: 'enableMediaSourceRendering', data: {} }));
};

/**
 * If we haven't yet received a "hello" message from the plugin, change its
 * size so that the user can confirm it if click-to-play is enabled, or can
 * see the "this plugin is disabled" message if it is actually disabled.
 * @private
 */
remoting.ClientPlugin.prototype.showPluginForClickToPlay_ = function() {
  if (!this.helloReceived_) {
    var width = 200;
    var height = 200;
    this.plugin_.style.width = width + 'px';
    this.plugin_.style.height = height + 'px';
    // Center the plugin just underneath the "Connnecting..." dialog.
    var dialog = document.getElementById('client-dialog');
    var dialogRect = dialog.getBoundingClientRect();
    this.plugin_.style.top = (dialogRect.bottom + 16) + 'px';
    this.plugin_.style.left = (window.innerWidth - width) / 2 + 'px';
    this.plugin_.style.position = 'fixed';
  }
};

/**
 * Undo the CSS rules needed to make the plugin clickable for click-to-play.
 * @private
 */
remoting.ClientPlugin.prototype.hidePluginForClickToPlay_ = function() {
  this.plugin_.style.width = '';
  this.plugin_.style.height = '';
  this.plugin_.style.top = '';
  this.plugin_.style.left = '';
  this.plugin_.style.position = '';
};
