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

/** @constructor */
remoting.ClientPluginMessage = function() {
  /** @type {string} */
  this.method = '';

  /** @type {Object<string,*>} */
  this.data = {};
};

/**
 * @param {Element} container The container for the embed element.
 * @param {Array<string>} requiredCapabilities The set of capabilties that the
 *     session must support for this application.
 * @constructor
 * @implements {remoting.ClientPlugin}
 */
remoting.ClientPluginImpl = function(container,
                                     requiredCapabilities) {
  this.plugin_ = remoting.ClientPluginImpl.createPluginElement_();
  this.plugin_.id = 'session-client-plugin';
  container.appendChild(this.plugin_);

  /** @private {Array<string>} */
  this.requiredCapabilities_ = requiredCapabilities;

  /** @private {remoting.ClientPlugin.ConnectionEventHandler} */
  this.connectionEventHandler_ = null;

  /**
   * @param {string} data Remote gnubbyd data.
   * @private
   */
  this.onGnubbyAuthHandler_ = function(data) {};
  /**
   * @param {string} url
   * @param {number} hotspotX
   * @param {number} hotspotY
   * @private
   */
  this.updateMouseCursorImage_ = function(url, hotspotX, hotspotY) {};
  /**
   * @param {string} data Remote cast extension message.
   * @private
   */
  this.onCastExtensionHandler_ = function(data) {};
  /** @private {?function({rects:Array<Array<number>>}):void} */
  this.debugRegionHandler_ = null;

  /** @private {number} */
  this.pluginApiVersion_ = -1;
  /** @private {Array<string>} */
  this.pluginApiFeatures_ = [];
  /** @private {number} */
  this.pluginApiMinVersion_ = -1;
  /** @private {!Array<string>} */
  this.capabilities_ = [];
  /** @private {boolean} */
  this.helloReceived_ = false;
  /** @private {function(boolean)|null} */
  this.onInitializedCallback_ = null;
  /** @private {function(string, string):void} */
  this.onPairingComplete_ = function(clientId, sharedSecret) {};
  /** @private {remoting.ClientSession.PerfStats} */
  this.perfStats_ = new remoting.ClientSession.PerfStats();

  /** @type {remoting.ClientPluginImpl} */
  var that = this;
  /** @param {Event} event Message event from the plugin. */
  this.plugin_.addEventListener('message', function(event) {
      that.handleMessage_(
          /** @type {remoting.ClientPluginMessage} */ (event.data));
    }, false);

  if (remoting.settings.CLIENT_PLUGIN_TYPE == 'native') {
    window.setTimeout(this.showPluginForClickToPlay_.bind(this), 500);
  }

  this.hostDesktop_ = new remoting.ClientPlugin.HostDesktopImpl(
      this, this.postMessage_.bind(this));

  /** @private {remoting.CredentialsProvider} */
  this.credentials_ = null;
};

/**
 * Creates plugin element without adding it to a container.
 *
 * @return {HTMLEmbedElement} Plugin element
 */
remoting.ClientPluginImpl.createPluginElement_ = function() {
  var plugin =
      /** @type {HTMLEmbedElement} */ (document.createElement('embed'));
  if (remoting.settings.CLIENT_PLUGIN_TYPE == 'pnacl') {
    plugin.src = 'remoting_client_pnacl.nmf';
    plugin.type = 'application/x-pnacl';
  } else if (remoting.settings.CLIENT_PLUGIN_TYPE == 'nacl') {
    plugin.src = 'remoting_client_nacl.nmf';
    plugin.type = 'application/x-nacl';
  } else {
    plugin.src = 'about://none';
    plugin.type = 'application/vnd.chromium.remoting-viewer';
  }
  plugin.width = '0';
  plugin.height = '0';
  plugin.tabIndex = 0;  // Required, otherwise focus() doesn't work.
  return plugin;
}

/**
 * Chromoting session API version (for this javascript).
 * This is compared with the plugin API version to verify that they are
 * compatible.
 *
 * @const
 * @private
 */
remoting.ClientPluginImpl.prototype.API_VERSION_ = 6;

/**
 * The oldest API version that we support.
 * This will differ from the |API_VERSION_| if we maintain backward
 * compatibility with older API versions.
 *
 * @const
 * @private
 */
remoting.ClientPluginImpl.prototype.API_MIN_VERSION_ = 5;

/**
 * @param {remoting.ClientPlugin.ConnectionEventHandler} handler
 */
remoting.ClientPluginImpl.prototype.setConnectionEventHandler =
    function(handler) {
  this.connectionEventHandler_ = handler;
};

/**
 * @param {function(string, number, number):void} handler
 */
remoting.ClientPluginImpl.prototype.setMouseCursorHandler = function(handler) {
  this.updateMouseCursorImage_ = handler;
};

/**
 * @param {?function({rects:Array<Array<number>>}):void} handler
 */
remoting.ClientPluginImpl.prototype.setDebugDirtyRegionHandler =
    function(handler) {
  this.debugRegionHandler_ = handler;
  this.plugin_.postMessage(JSON.stringify(
      { method: 'enableDebugRegion', data: { enable: handler != null } }));
};

/**
 * @param {string|remoting.ClientPluginMessage}
 *    rawMessage Message from the plugin.
 * @private
 */
remoting.ClientPluginImpl.prototype.handleMessage_ = function(rawMessage) {
  var message =
      /** @type {remoting.ClientPluginMessage} */
      ((typeof(rawMessage) == 'string') ? base.jsonParseSafe(rawMessage)
                                        : rawMessage);
  if (!message || !('method' in message) || !('data' in message)) {
    console.error('Received invalid message from the plugin:', rawMessage);
    return;
  }

  try {
    this.handleMessageMethod_(message);
  } catch(/** @type {*} */ e) {
    console.error(e);
  }
}

/**
 * @param {remoting.ClientPluginMessage}
 *    message Parsed message from the plugin.
 * @private
 */
remoting.ClientPluginImpl.prototype.handleMessageMethod_ = function(message) {
  /**
   * Splits a string into a list of words delimited by spaces.
   * @param {string} str String that should be split.
   * @return {!Array<string>} List of words.
   */
  var tokenize = function(str) {
    /** @type {Array<string>} */
    var tokens = str.match(/\S+/g);
    return tokens ? tokens : [];
  };

  if (this.connectionEventHandler_) {
    var handler = this.connectionEventHandler_;

    if (message.method == 'sendOutgoingIq') {
      handler.onOutgoingIq(base.getStringAttr(message.data, 'iq'));

    } else if (message.method == 'logDebugMessage') {
      handler.onDebugMessage(base.getStringAttr(message.data, 'message'));

    } else if (message.method == 'onConnectionStatus') {
      var state = remoting.ClientSession.State.fromString(
          base.getStringAttr(message.data, 'state'));
      var error = remoting.ClientSession.ConnectionError.fromString(
          base.getStringAttr(message.data, 'error'));
      handler.onConnectionStatusUpdate(state, error);

    } else if (message.method == 'onRouteChanged') {
      var channel = base.getStringAttr(message.data, 'channel');
      var connectionType = base.getStringAttr(message.data, 'connectionType');
      handler.onRouteChanged(channel, connectionType);

    } else if (message.method == 'onConnectionReady') {
      var ready = base.getBooleanAttr(message.data, 'ready');
      handler.onConnectionReady(ready);

    } else if (message.method == 'setCapabilities') {
      /** @type {!Array<string>} */
      var capabilities = tokenize(
          base.getStringAttr(message.data, 'capabilities'));
      handler.onSetCapabilities(capabilities);

    } else if (message.method == 'extensionMessage') {
      var extMsgType = base.getStringAttr(message.data, 'type');
      var extMsgData = base.getStringAttr(message.data, 'data');
      handler.onExtensionMessage(extMsgType, extMsgData);
    }
  }

  if (message.method == 'hello') {
    // Resize in case we had to enlarge it to support click-to-play.
    this.hidePluginForClickToPlay_();
    this.pluginApiVersion_ = base.getNumberAttr(message.data, 'apiVersion');
    this.pluginApiMinVersion_ =
        base.getNumberAttr(message.data, 'apiMinVersion');

    if (this.pluginApiVersion_ >= 7) {
      this.pluginApiFeatures_ =
          tokenize(base.getStringAttr(message.data, 'apiFeatures'));

      // Negotiate capabilities.
      /** @type {!Array<string>} */
      var supportedCapabilities = [];
      if ('supportedCapabilities' in message.data) {
        supportedCapabilities =
            tokenize(base.getStringAttr(message.data, 'supportedCapabilities'));
      }
      // At the moment the webapp does not recognize any of
      // 'requestedCapabilities' capabilities (so they all should be disabled)
      // and do not care about any of 'supportedCapabilities' capabilities (so
      // they all can be enabled).
      // All the required capabilities (specified by the app) are added to this.
      this.capabilities_ = supportedCapabilities.concat(
          this.requiredCapabilities_);
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

  } else if (message.method == 'onDesktopSize') {
    this.hostDesktop_.onSizeUpdated(message);
  } else if (message.method == 'onDesktopShape') {
    this.hostDesktop_.onShapeUpdated(message);
  } else if (message.method == 'onPerfStats') {
    // Return value is ignored. These calls will throw an error if the value
    // is not a number.
    base.getNumberAttr(message.data, 'videoBandwidth');
    base.getNumberAttr(message.data, 'videoFrameRate');
    base.getNumberAttr(message.data, 'captureLatency');
    base.getNumberAttr(message.data, 'encodeLatency');
    base.getNumberAttr(message.data, 'decodeLatency');
    base.getNumberAttr(message.data, 'renderLatency');
    base.getNumberAttr(message.data, 'roundtripLatency');
    this.perfStats_ =
        /** @type {remoting.ClientSession.PerfStats} */ (message.data);

  } else if (message.method == 'injectClipboardItem') {
    var mimetype = base.getStringAttr(message.data, 'mimeType');
    var item = base.getStringAttr(message.data, 'item');
    if (remoting.clipboard) {
      remoting.clipboard.fromHost(mimetype, item);
    }

  } else if (message.method == 'onFirstFrameReceived') {
    if (remoting.clientSession) {
      remoting.clientSession.onFirstFrameReceived();
    }

  } else if (message.method == 'fetchPin') {
    // The pairingSupported value in the dictionary indicates whether both
    // client and host support pairing. If the client doesn't support pairing,
    // then the value won't be there at all, so give it a default of false.
    var pairingSupported = base.getBooleanAttr(message.data, 'pairingSupported',
                                          false);
    this.credentials_.getPIN(pairingSupported).then(
        this.onPinFetched_.bind(this)
    );

  } else if (message.method == 'fetchThirdPartyToken') {
    var tokenUrl = base.getStringAttr(message.data, 'tokenUrl');
    var hostPublicKey = base.getStringAttr(message.data, 'hostPublicKey');
    var scope = base.getStringAttr(message.data, 'scope');
    this.credentials_.getThirdPartyToken(tokenUrl, hostPublicKey, scope).then(
      this.onThirdPartyTokenFetched_.bind(this)
    );
  } else if (message.method == 'pairingResponse') {
    var clientId = base.getStringAttr(message.data, 'clientId');
    var sharedSecret = base.getStringAttr(message.data, 'sharedSecret');
    this.onPairingComplete_(clientId, sharedSecret);

  } else if (message.method == 'unsetCursorShape') {
    this.updateMouseCursorImage_('', 0, 0);

  } else if (message.method == 'setCursorShape') {
    var width = base.getNumberAttr(message.data, 'width');
    var height = base.getNumberAttr(message.data, 'height');
    var hotspotX = base.getNumberAttr(message.data, 'hotspotX');
    var hotspotY = base.getNumberAttr(message.data, 'hotspotY');
    var srcArrayBuffer = base.getObjectAttr(message.data, 'data');

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
    this.updateMouseCursorImage_(canvas.toDataURL(), hotspotX, hotspotY);

  } else if (message.method == 'onDebugRegion') {
    if (this.debugRegionHandler_) {
      this.debugRegionHandler_(
          /** @type {{rects: Array<(Array<number>)>}} **/(message.data));
    }
  }
};

/**
 * Deletes the plugin.
 */
remoting.ClientPluginImpl.prototype.dispose = function() {
  if (this.plugin_) {
    this.plugin_.parentNode.removeChild(this.plugin_);
    this.plugin_ = null;
  }
};

/**
 * @return {HTMLEmbedElement} HTML element that corresponds to the plugin.
 */
remoting.ClientPluginImpl.prototype.element = function() {
  return this.plugin_;
};

/**
 * @param {function(boolean): void} onDone
 */
remoting.ClientPluginImpl.prototype.initialize = function(onDone) {
  if (this.helloReceived_) {
    onDone(true);
  } else {
    this.onInitializedCallback_ = onDone;
  }
};

/**
 * @return {boolean} True if the plugin and web-app versions are compatible.
 */
remoting.ClientPluginImpl.prototype.isSupportedVersion = function() {
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
remoting.ClientPluginImpl.prototype.hasFeature = function(feature) {
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
remoting.ClientPluginImpl.prototype.isInjectKeyEventSupported = function() {
  return this.pluginApiVersion_ >= 6;
};

/**
 * @param {string} iq Incoming IQ stanza.
 */
remoting.ClientPluginImpl.prototype.onIncomingIq = function(iq) {
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
 * @param {remoting.Host} host The host to connect to.
 * @param {string} localJid Local jid.
 * @param {remoting.CredentialsProvider} credentialsProvider
 */
remoting.ClientPluginImpl.prototype.connect =
    function(host, localJid, credentialsProvider) {
  var keyFilter = '';
  if (remoting.platformIsMac()) {
    keyFilter = 'mac';
  } else if (remoting.platformIsChromeOS()) {
    keyFilter = 'cros';
  }

  // Use PPB_VideoDecoder API only in Chrome 43 and above. It is broken in
  // previous versions of Chrome, see crbug.com/459103 and crbug.com/463577 .
  var enableVideoDecodeRenderer =
      parseInt((remoting.getChromeVersion() || '0').split('.')[0], 10) >= 43;
  this.plugin_.postMessage(JSON.stringify(
      { method: 'delegateLargeCursors', data: {} }));
  var methods = 'third_party,spake2_pair,spake2_hmac,spake2_plain';
  this.credentials_ = credentialsProvider;
  this.useAsyncPinDialog_();
  this.plugin_.postMessage(JSON.stringify(
    { method: 'connect', data: {
        hostJid: host.jabberId,
        hostPublicKey: host.publicKey,
        localJid: localJid,
        sharedSecret: '',
        authenticationMethods: methods,
        authenticationTag: host.hostId,
        capabilities: this.capabilities_.join(" "),
        clientPairingId: credentialsProvider.getPairingInfo().clientId,
        clientPairedSecret: credentialsProvider.getPairingInfo().sharedSecret,
        keyFilter: keyFilter,
        enableVideoDecodeRenderer: enableVideoDecodeRenderer
      }
    }));
};

/**
 * Release all currently pressed keys.
 */
remoting.ClientPluginImpl.prototype.releaseAllKeys = function() {
  this.plugin_.postMessage(JSON.stringify(
      { method: 'releaseAllKeys', data: {} }));
};

/**
 * Send a key event to the host.
 *
 * @param {number} usbKeycode The USB-style code of the key to inject.
 * @param {boolean} pressed True to inject a key press, False for a release.
 */
remoting.ClientPluginImpl.prototype.injectKeyEvent =
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
remoting.ClientPluginImpl.prototype.remapKey =
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
remoting.ClientPluginImpl.prototype.trapKey = function(keycode, trap) {
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
remoting.ClientPluginImpl.prototype.getPerfStats = function() {
  return this.perfStats_;
};

/**
 * Sends a clipboard item to the host.
 *
 * @param {string} mimeType The MIME type of the clipboard item.
 * @param {string} item The clipboard item.
 */
remoting.ClientPluginImpl.prototype.sendClipboardItem =
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
remoting.ClientPluginImpl.prototype.notifyClientResolution =
    function(width, height, device_scale) {
  this.hostDesktop_.resize(width, height, device_scale);
};

/**
 * Requests that the host pause or resume sending video updates.
 *
 * @param {boolean} pause True to suspend video updates, false otherwise.
 */
remoting.ClientPluginImpl.prototype.pauseVideo =
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
remoting.ClientPluginImpl.prototype.pauseAudio =
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
remoting.ClientPluginImpl.prototype.setLosslessEncode =
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
remoting.ClientPluginImpl.prototype.setLosslessColor =
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
 * @private
 */
remoting.ClientPluginImpl.prototype.onPinFetched_ =
    function(pin) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.ASYNC_PIN)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'onPinFetched', data: { pin: pin }}));
};

/**
 * Tells the plugin to ask for the PIN asynchronously.
 * @private
 */
remoting.ClientPluginImpl.prototype.useAsyncPinDialog_ =
    function() {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.ASYNC_PIN)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'useAsyncPinDialog', data: {} }));
};

/**
 * Allows automatic mouse-lock.
 */
remoting.ClientPluginImpl.prototype.allowMouseLock = function() {
  this.plugin_.postMessage(JSON.stringify(
      { method: 'allowMouseLock', data: {} }));
};

/**
 * Sets the third party authentication token and shared secret.
 *
 * @param {remoting.ThirdPartyToken} token
 * @private
 */
remoting.ClientPluginImpl.prototype.onThirdPartyTokenFetched_ = function(
    token) {
  this.plugin_.postMessage(JSON.stringify(
    { method: 'onThirdPartyTokenFetched',
      data: { token: token.token, sharedSecret: token.secret}}));
};

/**
 * Request pairing with the host for PIN-less authentication.
 *
 * @param {string} clientName The human-readable name of the client.
 * @param {function(string, string):void} onDone, Callback to receive the
 *     client id and shared secret when they are available.
 */
remoting.ClientPluginImpl.prototype.requestPairing =
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
remoting.ClientPluginImpl.prototype.sendClientMessage =
    function(type, message) {
  if (!this.hasFeature(remoting.ClientPlugin.Feature.EXTENSION_MESSAGE)) {
    return;
  }
  this.plugin_.postMessage(JSON.stringify(
      { method: 'extensionMessage',
        data: { type: type, data: message } }));

};

remoting.ClientPluginImpl.prototype.hostDesktop = function() {
  return this.hostDesktop_;
};

/**
 * If we haven't yet received a "hello" message from the plugin, change its
 * size so that the user can confirm it if click-to-play is enabled, or can
 * see the "this plugin is disabled" message if it is actually disabled.
 * @private
 */
remoting.ClientPluginImpl.prototype.showPluginForClickToPlay_ = function() {
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
remoting.ClientPluginImpl.prototype.hidePluginForClickToPlay_ = function() {
  this.plugin_.style.width = '';
  this.plugin_.style.height = '';
  this.plugin_.style.top = '';
  this.plugin_.style.left = '';
  this.plugin_.style.position = '';
};

/**
 * Callback passed to submodules to post a message to the plugin.
 *
 * @param {Object} message
 * @private
 */
remoting.ClientPluginImpl.prototype.postMessage_ = function(message) {
  if (this.plugin_ && this.plugin_.postMessage) {
    this.plugin_.postMessage(JSON.stringify(message));
  }
};

/**
 * @constructor
 * @implements {remoting.ClientPluginFactory}
 */
remoting.DefaultClientPluginFactory = function() {};

/**
 * @param {Element} container
 * @param {Array<string>} requiredCapabilities
 * @return {remoting.ClientPlugin}
 */
remoting.DefaultClientPluginFactory.prototype.createPlugin =
    function(container, requiredCapabilities) {
  return new remoting.ClientPluginImpl(container,
                                       requiredCapabilities);
};

remoting.DefaultClientPluginFactory.prototype.preloadPlugin = function() {
  if (remoting.settings.CLIENT_PLUGIN_TYPE != 'pnacl') {
    return;
  }

  var plugin = remoting.ClientPluginImpl.createPluginElement_();
  plugin.addEventListener(
      'loadend', function() { document.body.removeChild(plugin); }, false);
  document.body.appendChild(plugin);
};
