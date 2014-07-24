// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling creation and teardown of a remoting client session.
 *
 * The ClientSession class controls lifetime of the client plugin
 * object and provides the plugin with the functionality it needs to
 * establish connection. Specifically it:
 *  - Delivers incoming/outgoing signaling messages,
 *  - Adjusts plugin size and position when destop resolution changes,
 *
 * This class should not access the plugin directly, instead it should
 * do it through ClientPlugin class which abstracts plugin version
 * differences.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {HTMLElement} container Container element for the client view.
 * @param {string} hostDisplayName A human-readable name for the host.
 * @param {string} accessCode The IT2Me access code. Blank for Me2Me.
 * @param {function(boolean, function(string): void): void} fetchPin
 *     Called by Me2Me connections when a PIN needs to be obtained
 *     interactively.
 * @param {function(string, string, string,
 *                  function(string, string): void): void}
 *     fetchThirdPartyToken Called by Me2Me connections when a third party
 *     authentication token must be obtained.
 * @param {string} authenticationMethods Comma-separated list of
 *     authentication methods the client should attempt to use.
 * @param {string} hostId The host identifier for Me2Me, or empty for IT2Me.
 *     Mixed into authentication hashes for some authentication methods.
 * @param {string} hostJid The jid of the host to connect to.
 * @param {string} hostPublicKey The base64 encoded version of the host's
 *     public key.
 * @param {remoting.ClientSession.Mode} mode The mode of this connection.
 * @param {string} clientPairingId For paired Me2Me connections, the
 *     pairing id for this client, as issued by the host.
 * @param {string} clientPairedSecret For paired Me2Me connections, the
 *     paired secret for this client, as issued by the host.
 * @constructor
 * @extends {base.EventSource}
 */
remoting.ClientSession = function(container, hostDisplayName, accessCode,
                                  fetchPin, fetchThirdPartyToken,
                                  authenticationMethods, hostId, hostJid,
                                  hostPublicKey, mode, clientPairingId,
                                  clientPairedSecret) {
  /** @private */
  this.state_ = remoting.ClientSession.State.CREATED;

  /** @private */
  this.error_ = remoting.Error.NONE;

  /** @type {HTMLElement}
    * @private */
  this.container_ = container;

  /** @private */
  this.hostDisplayName_ = hostDisplayName;
  /** @private */
  this.hostJid_ = hostJid;
  /** @private */
  this.hostPublicKey_ = hostPublicKey;
  /** @private */
  this.accessCode_ = accessCode;
  /** @private */
  this.fetchPin_ = fetchPin;
  /** @private */
  this.fetchThirdPartyToken_ = fetchThirdPartyToken;
  /** @private */
  this.authenticationMethods_ = authenticationMethods;
  /** @private */
  this.hostId_ = hostId;
  /** @private */
  this.mode_ = mode;
  /** @private */
  this.clientPairingId_ = clientPairingId;
  /** @private */
  this.clientPairedSecret_ = clientPairedSecret;
  /** @private */
  this.sessionId_ = '';
  /** @type {remoting.ClientPlugin}
    * @private */
  this.plugin_ = null;
  /** @private */
  this.shrinkToFit_ = true;
  /** @private */
  this.resizeToClient_ = true;
  /** @private */
  this.remapKeys_ = '';
  /** @private */
  this.hasReceivedFrame_ = false;
  this.logToServer = new remoting.LogToServer();

  /** @type {number?} @private */
  this.notifyClientResolutionTimer_ = null;
  /** @type {number?} @private */
  this.bumpScrollTimer_ = null;

  /**
   * Allow host-offline error reporting to be suppressed in situations where it
   * would not be useful, for example, when using a cached host JID.
   *
   * @type {boolean} @private
   */
  this.logHostOfflineErrors_ = true;

  /** @private */
  this.callPluginLostFocus_ = this.pluginLostFocus_.bind(this);
  /** @private */
  this.callPluginGotFocus_ = this.pluginGotFocus_.bind(this);
  /** @private */
  this.callToggleFullScreen_ = remoting.fullscreen.toggle.bind(
      remoting.fullscreen);
  /** @private */
  this.callOnFullScreenChanged_ = this.onFullScreenChanged_.bind(this)

  /** @private */
  this.screenOptionsMenu_ = new remoting.MenuButton(
      document.getElementById('screen-options-menu'),
      this.onShowOptionsMenu_.bind(this));
  /** @private */
  this.sendKeysMenu_ = new remoting.MenuButton(
      document.getElementById('send-keys-menu')
  );

  /** @type {HTMLMediaElement} @private */
  this.video_ = null;

  /** @type {Element} @private */
  this.mouseCursorOverlay_ =
      this.container_.querySelector('.mouse-cursor-overlay');

  /** @type {Element} */
  var img = this.mouseCursorOverlay_;
  /** @param {Event} event @private */
  this.updateMouseCursorPosition_ = function(event) {
    img.style.top = event.y + 'px';
    img.style.left = event.x + 'px';
  };

  /** @type {HTMLElement} @private */
  this.resizeToClientButton_ =
      document.getElementById('screen-resize-to-client');
  /** @type {HTMLElement} @private */
  this.shrinkToFitButton_ = document.getElementById('screen-shrink-to-fit');
  /** @type {HTMLElement} @private */
  this.fullScreenButton_ = document.getElementById('toggle-full-screen');

  /** @type {remoting.GnubbyAuthHandler} @private */
  this.gnubbyAuthHandler_ = null;

  if (this.mode_ == remoting.ClientSession.Mode.IT2ME) {
    // Resize-to-client is not supported for IT2Me hosts.
    this.resizeToClientButton_.hidden = true;
  } else {
    this.resizeToClientButton_.hidden = false;
  }

  this.fullScreenButton_.addEventListener(
      'click', this.callToggleFullScreen_, false);
  this.defineEvents(Object.keys(remoting.ClientSession.Events));
};

base.extend(remoting.ClientSession, base.EventSource);

/** @enum {string} */
remoting.ClientSession.Events = {
  stateChanged: 'stateChanged',
  videoChannelStateChanged: 'videoChannelStateChanged'
};

/**
 * Get host display name.
 *
 * @return {string}
 */
remoting.ClientSession.prototype.getHostDisplayName = function() {
  return this.hostDisplayName_;
};

/**
 * Called when the window or desktop size or the scaling settings change,
 * to set the scroll-bar visibility.
 *
 * TODO(jamiewalch): crbug.com/252796: Remove this once crbug.com/240772 is
 * fixed.
 */
remoting.ClientSession.prototype.updateScrollbarVisibility = function() {
  var needsVerticalScroll = false;
  var needsHorizontalScroll = false;
  if (!this.shrinkToFit_) {
    // Determine whether or not horizontal or vertical scrollbars are
    // required, taking into account their width.
    var clientArea = this.getClientArea_();
    needsVerticalScroll = clientArea.height < this.plugin_.desktopHeight;
    needsHorizontalScroll = clientArea.width < this.plugin_.desktopWidth;
    var kScrollBarWidth = 16;
    if (needsHorizontalScroll && !needsVerticalScroll) {
      needsVerticalScroll =
          clientArea.height - kScrollBarWidth < this.plugin_.desktopHeight;
    } else if (!needsHorizontalScroll && needsVerticalScroll) {
      needsHorizontalScroll =
          clientArea.width - kScrollBarWidth < this.plugin_.desktopWidth;
    }
  }

  var scroller = document.getElementById('scroller');
  if (needsHorizontalScroll) {
    scroller.classList.remove('no-horizontal-scroll');
  } else {
    scroller.classList.add('no-horizontal-scroll');
  }
  if (needsVerticalScroll) {
    scroller.classList.remove('no-vertical-scroll');
  } else {
    scroller.classList.add('no-vertical-scroll');
  }
};

/**
 * @return {boolean} True if shrink-to-fit is enabled; false otherwise.
 */
remoting.ClientSession.prototype.getShrinkToFit = function() {
  return this.shrinkToFit_;
};

/**
 * @return {boolean} True if resize-to-client is enabled; false otherwise.
 */
remoting.ClientSession.prototype.getResizeToClient = function() {
  return this.resizeToClient_;
};

// Note that the positive values in both of these enums are copied directly
// from chromoting_scriptable_object.h and must be kept in sync. The negative
// values represent state transitions that occur within the web-app that have
// no corresponding plugin state transition.
/** @enum {number} */
remoting.ClientSession.State = {
  CONNECTION_CANCELED: -3,  // Connection closed (gracefully) before connecting.
  CONNECTION_DROPPED: -2,  // Succeeded, but subsequently closed with an error.
  CREATED: -1,
  UNKNOWN: 0,
  CONNECTING: 1,
  INITIALIZING: 2,
  CONNECTED: 3,
  CLOSED: 4,
  FAILED: 5
};

/**
 * @param {string} state The state name.
 * @return {remoting.ClientSession.State} The session state enum value.
 */
remoting.ClientSession.State.fromString = function(state) {
  if (!remoting.ClientSession.State.hasOwnProperty(state)) {
    throw "Invalid ClientSession.State: " + state;
  }
  return remoting.ClientSession.State[state];
};

/**
  @constructor
  @param {remoting.ClientSession.State} current
  @param {remoting.ClientSession.State} previous
*/
remoting.ClientSession.StateEvent = function(current, previous) {
  /** @type {remoting.ClientSession.State} */
  this.previous = previous

  /** @type {remoting.ClientSession.State} */
  this.current = current;
};

/** @enum {number} */
remoting.ClientSession.ConnectionError = {
  UNKNOWN: -1,
  NONE: 0,
  HOST_IS_OFFLINE: 1,
  SESSION_REJECTED: 2,
  INCOMPATIBLE_PROTOCOL: 3,
  NETWORK_FAILURE: 4,
  HOST_OVERLOAD: 5
};

/**
 * @param {string} error The connection error name.
 * @return {remoting.ClientSession.ConnectionError} The connection error enum.
 */
remoting.ClientSession.ConnectionError.fromString = function(error) {
  if (!remoting.ClientSession.ConnectionError.hasOwnProperty(error)) {
    console.error('Unexpected ClientSession.ConnectionError string: ', error);
    return remoting.ClientSession.ConnectionError.UNKNOWN;
  }
  return remoting.ClientSession.ConnectionError[error];
}

// The mode of this session.
/** @enum {number} */
remoting.ClientSession.Mode = {
  IT2ME: 0,
  ME2ME: 1
};

/**
 * Type used for performance statistics collected by the plugin.
 * @constructor
 */
remoting.ClientSession.PerfStats = function() {};
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.videoBandwidth;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.videoFrameRate;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.captureLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.encodeLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.decodeLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.renderLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.roundtripLatency;

// Keys for connection statistics.
remoting.ClientSession.STATS_KEY_VIDEO_BANDWIDTH = 'videoBandwidth';
remoting.ClientSession.STATS_KEY_VIDEO_FRAME_RATE = 'videoFrameRate';
remoting.ClientSession.STATS_KEY_CAPTURE_LATENCY = 'captureLatency';
remoting.ClientSession.STATS_KEY_ENCODE_LATENCY = 'encodeLatency';
remoting.ClientSession.STATS_KEY_DECODE_LATENCY = 'decodeLatency';
remoting.ClientSession.STATS_KEY_RENDER_LATENCY = 'renderLatency';
remoting.ClientSession.STATS_KEY_ROUNDTRIP_LATENCY = 'roundtripLatency';

// Keys for per-host settings.
remoting.ClientSession.KEY_REMAP_KEYS = 'remapKeys';
remoting.ClientSession.KEY_RESIZE_TO_CLIENT = 'resizeToClient';
remoting.ClientSession.KEY_SHRINK_TO_FIT = 'shrinkToFit';

/**
 * The id of the client plugin
 *
 * @const
 */
remoting.ClientSession.prototype.PLUGIN_ID = 'session-client-plugin';

/**
 * Set of capabilities for which hasCapability_() can be used to test.
 *
 * @enum {string}
 */
remoting.ClientSession.Capability = {
  // When enabled this capability causes the client to send its screen
  // resolution to the host once connection has been established. See
  // this.plugin_.notifyClientResolution().
  SEND_INITIAL_RESOLUTION: 'sendInitialResolution',
  RATE_LIMIT_RESIZE_REQUESTS: 'rateLimitResizeRequests'
};

/**
 * The set of capabilities negotiated between the client and host.
 * @type {Array.<string>}
 * @private
 */
remoting.ClientSession.prototype.capabilities_ = null;

/**
 * @param {remoting.ClientSession.Capability} capability The capability to test
 *     for.
 * @return {boolean} True if the capability has been negotiated between
 *     the client and host.
 * @private
 */
remoting.ClientSession.prototype.hasCapability_ = function(capability) {
  if (this.capabilities_ == null)
    return false;

  return this.capabilities_.indexOf(capability) > -1;
};

/**
 * @param {string} id Id to use for the plugin element .
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @return {remoting.ClientPlugin} Create plugin object for the locally
 * installed plugin.
 */
remoting.ClientSession.prototype.createClientPlugin_ =
    function(id, onExtensionMessage) {
  var plugin = /** @type {remoting.ViewerPlugin} */
      document.createElement('embed');

  plugin.id = id;
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

  plugin.width = 0;
  plugin.height = 0;
  plugin.tabIndex = 0;  // Required, otherwise focus() doesn't work.
  this.container_.querySelector('.client-plugin-container').appendChild(plugin);

  return new remoting.ClientPlugin(plugin, onExtensionMessage);
};

/**
 * Callback function called when the plugin element gets focus.
 */
remoting.ClientSession.prototype.pluginGotFocus_ = function() {
  remoting.clipboard.initiateToHost();
};

/**
 * Callback function called when the plugin element loses focus.
 */
remoting.ClientSession.prototype.pluginLostFocus_ = function() {
  if (this.plugin_) {
    // Release all keys to prevent them becoming 'stuck down' on the host.
    this.plugin_.releaseAllKeys();
    if (this.plugin_.element()) {
      // Focus should stay on the element, not (for example) the toolbar.
      // Due to crbug.com/246335, we can't restore the focus immediately,
      // otherwise the plugin gets confused about whether or not it has focus.
      window.setTimeout(
          this.plugin_.element().focus.bind(this.plugin_.element()),
          0);
    }
  }
};

/**
 * Adds <embed> element to |container| and readies the sesion object.
 *
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 */
remoting.ClientSession.prototype.createPluginAndConnect =
    function(onExtensionMessage) {
  this.plugin_ = this.createClientPlugin_(this.PLUGIN_ID, onExtensionMessage);
  remoting.HostSettings.load(this.hostId_,
                             this.onHostSettingsLoaded_.bind(this));
};

/**
 * @param {Object.<string>} options The current options for the host, or {}
 *     if this client has no saved settings for the host.
 * @private
 */
remoting.ClientSession.prototype.onHostSettingsLoaded_ = function(options) {
  if (remoting.ClientSession.KEY_REMAP_KEYS in options &&
      typeof(options[remoting.ClientSession.KEY_REMAP_KEYS]) ==
          'string') {
    this.remapKeys_ = /** @type {string} */
        options[remoting.ClientSession.KEY_REMAP_KEYS];
  }
  if (remoting.ClientSession.KEY_RESIZE_TO_CLIENT in options &&
      typeof(options[remoting.ClientSession.KEY_RESIZE_TO_CLIENT]) ==
          'boolean') {
    this.resizeToClient_ = /** @type {boolean} */
        options[remoting.ClientSession.KEY_RESIZE_TO_CLIENT];
  }
  if (remoting.ClientSession.KEY_SHRINK_TO_FIT in options &&
      typeof(options[remoting.ClientSession.KEY_SHRINK_TO_FIT]) ==
          'boolean') {
    this.shrinkToFit_ = /** @type {boolean} */
        options[remoting.ClientSession.KEY_SHRINK_TO_FIT];
  }

  /** @param {boolean} result */
  this.plugin_.initialize(this.onPluginInitialized_.bind(this));
};

/**
 * Constrains the focus to the plugin element.
 * @private
 */
remoting.ClientSession.prototype.setFocusHandlers_ = function() {
  this.plugin_.element().addEventListener(
      'focus', this.callPluginGotFocus_, false);
  this.plugin_.element().addEventListener(
      'blur', this.callPluginLostFocus_, false);
  this.plugin_.element().focus();
};

/**
 * @param {remoting.Error} error
 */
remoting.ClientSession.prototype.resetWithError_ = function(error) {
  this.plugin_.cleanup();
  delete this.plugin_;
  this.error_ = error;
  this.setState_(remoting.ClientSession.State.FAILED);
}

/**
 * @param {boolean} initialized
 */
remoting.ClientSession.prototype.onPluginInitialized_ = function(initialized) {
  if (!initialized) {
    console.error('ERROR: remoting plugin not loaded');
    this.resetWithError_(remoting.Error.MISSING_PLUGIN);
    return;
  }

  if (!this.plugin_.isSupportedVersion()) {
    this.resetWithError_(remoting.Error.BAD_PLUGIN_VERSION);
    return;
  }

  // Show the Send Keys menu only if the plugin has the injectKeyEvent feature,
  // and the Ctrl-Alt-Del button only in Me2Me mode.
  if (!this.plugin_.hasFeature(
          remoting.ClientPlugin.Feature.INJECT_KEY_EVENT)) {
    var sendKeysElement = document.getElementById('send-keys-menu');
    sendKeysElement.hidden = true;
  } else if (this.mode_ != remoting.ClientSession.Mode.ME2ME) {
    var sendCadElement = document.getElementById('send-ctrl-alt-del');
    sendCadElement.hidden = true;
  }

  // Apply customized key remappings if the plugin supports remapKeys.
  if (this.plugin_.hasFeature(remoting.ClientPlugin.Feature.REMAP_KEY)) {
    this.applyRemapKeys_(true);
  }


  // Enable MediaSource-based rendering on Chrome 37 and above.
  var chromeVersionMajor =
      parseInt((remoting.getChromeVersion() || '0').split('.')[0], 10);
  if (chromeVersionMajor >= 37 &&
      this.plugin_.hasFeature(
          remoting.ClientPlugin.Feature.MEDIA_SOURCE_RENDERING)) {
    this.video_ = /** @type {HTMLMediaElement} */(
        this.container_.querySelector('video'));
    // Make sure that the <video> element is hidden until we get the first
    // frame.
    this.video_.style.width = '0px';
    this.video_.style.height = '0px';

    var renderer = new remoting.MediaSourceRenderer(this.video_);
    this.plugin_.enableMediaSourceRendering(renderer);
    this.container_.classList.add('mediasource-rendering');
  } else {
    this.container_.classList.remove('mediasource-rendering');
  }

  /** @param {string} msg The IQ stanza to send. */
  this.plugin_.onOutgoingIqHandler = this.sendIq_.bind(this);
  /** @param {string} msg The message to log. */
  this.plugin_.onDebugMessageHandler = function(msg) {
    console.log('plugin: ' + msg.trimRight());
  };

  this.plugin_.onConnectionStatusUpdateHandler =
      this.onConnectionStatusUpdate_.bind(this);
  this.plugin_.onConnectionReadyHandler = this.onConnectionReady_.bind(this);
  this.plugin_.onDesktopSizeUpdateHandler =
      this.onDesktopSizeChanged_.bind(this);
  this.plugin_.onSetCapabilitiesHandler = this.onSetCapabilities_.bind(this);
  this.plugin_.onGnubbyAuthHandler = this.processGnubbyAuthMessage_.bind(this);
  this.plugin_.updateMouseCursorImage = this.updateMouseCursorImage_.bind(this);
  this.initiateConnection_();
};

/**
 * Deletes the <embed> element from the container, without sending a
 * session_terminate request.  This is to be called when the session was
 * disconnected by the Host.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.removePlugin = function() {
  if (this.plugin_) {
    this.plugin_.element().removeEventListener(
        'focus', this.callPluginGotFocus_, false);
    this.plugin_.element().removeEventListener(
        'blur', this.callPluginLostFocus_, false);
    this.plugin_.cleanup();
    this.plugin_ = null;
  }

  // Delete event handlers that aren't relevent when not connected.
  this.fullScreenButton_.removeEventListener(
      'click', this.callToggleFullScreen_, false);

  // Leave full-screen mode, and stop listening for related events.
  var listener = this.callOnFullScreenChanged_;
  remoting.fullscreen.syncWithMaximize(false);
  remoting.fullscreen.activate(
      false,
      function() {
        remoting.fullscreen.removeListener(listener);
      });
  if (remoting.windowFrame) {
    remoting.windowFrame.setClientSession(null);
  } else {
    remoting.toolbar.setClientSession(null);
  }

  // Remove mediasource-rendering class from the container - this will also
  // hide the <video> element.
  this.container_.classList.remove('mediasource-rendering');

  this.container_.removeEventListener('mousemove',
                                      this.updateMouseCursorPosition_,
                                      true);
};

/**
 * Disconnect the current session with a particular |error|.  The session will
 * raise a |stateChanged| event in response to it.  The caller should then call
 * |cleanup| to remove and destroy the <embed> element.
 *
 * @param {remoting.Error} error The reason for the disconnection.  Use
 *    remoting.Error.NONE if there is no error.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.disconnect = function(error) {
  var state = (error == remoting.Error.NONE) ?
                  remoting.ClientSession.State.CLOSED :
                  remoting.ClientSession.State.FAILED;

  // The plugin won't send a state change notification, so we explicitly log
  // the fact that the connection has closed.
  this.logToServer.logClientSessionStateChange(state, error, this.mode_);
  this.error_ = error;
  this.setState_(state);
};

/**
 * Deletes the <embed> element from the container and disconnects.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.cleanup = function() {
  remoting.wcsSandbox.setOnIq(null);
  this.sendIq_(
      '<cli:iq ' +
          'to="' + this.hostJid_ + '" ' +
          'type="set" ' +
          'id="session-terminate" ' +
          'xmlns:cli="jabber:client">' +
        '<jingle ' +
            'xmlns="urn:xmpp:jingle:1" ' +
            'action="session-terminate" ' +
            'sid="' + this.sessionId_ + '">' +
          '<reason><success/></reason>' +
        '</jingle>' +
      '</cli:iq>');
  this.removePlugin();
};

/**
 * @return {remoting.ClientSession.Mode} The current state.
 */
remoting.ClientSession.prototype.getMode = function() {
  return this.mode_;
};

/**
 * @return {remoting.ClientSession.State} The current state.
 */
remoting.ClientSession.prototype.getState = function() {
  return this.state_;
};

/**
 * @return {remoting.Error} The current error code.
 */
remoting.ClientSession.prototype.getError = function() {
  return this.error_;
};

/**
 * Sends a key combination to the remoting client, by sending down events for
 * the given keys, followed by up events in reverse order.
 *
 * @private
 * @param {[number]} keys Key codes to be sent.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendKeyCombination_ = function(keys) {
  for (var i = 0; i < keys.length; i++) {
    this.plugin_.injectKeyEvent(keys[i], true);
  }
  for (var i = 0; i < keys.length; i++) {
    this.plugin_.injectKeyEvent(keys[i], false);
  }
}

/**
 * Sends a Ctrl-Alt-Del sequence to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendCtrlAltDel = function() {
  console.log('Sending Ctrl-Alt-Del.');
  this.sendKeyCombination_([0x0700e0, 0x0700e2, 0x07004c]);
}

/**
 * Sends a Print Screen keypress to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendPrintScreen = function() {
  console.log('Sending Print Screen.');
  this.sendKeyCombination_([0x070046]);
}

/**
 * Sets and stores the key remapping setting for the current host.
 *
 * @param {string} remappings Comma separated list of key remappings.
 */
remoting.ClientSession.prototype.setRemapKeys = function(remappings) {
  // Cancel any existing remappings and apply the new ones.
  this.applyRemapKeys_(false);
  this.remapKeys_ = remappings;
  this.applyRemapKeys_(true);

  // Save the new remapping setting.
  var options = {};
  options[remoting.ClientSession.KEY_REMAP_KEYS] = this.remapKeys_;
  remoting.HostSettings.save(this.hostId_, options);
}

/**
 * Applies the configured key remappings to the session, or resets them.
 *
 * @param {boolean} apply True to apply remappings, false to cancel them.
 */
remoting.ClientSession.prototype.applyRemapKeys_ = function(apply) {
  // By default, under ChromeOS, remap the right Control key to the right
  // Win / Cmd key.
  var remapKeys = this.remapKeys_;
  if (remapKeys == '' && remoting.runningOnChromeOS()) {
    remapKeys = '0x0700e4>0x0700e7';
  }

  if (remapKeys == '') {
    return;
  }

  var remappings = remapKeys.split(',');
  for (var i = 0; i < remappings.length; ++i) {
    var keyCodes = remappings[i].split('>');
    if (keyCodes.length != 2) {
      console.log('bad remapKey: ' + remappings[i]);
      continue;
    }
    var fromKey = parseInt(keyCodes[0], 0);
    var toKey = parseInt(keyCodes[1], 0);
    if (!fromKey || !toKey) {
      console.log('bad remapKey code: ' + remappings[i]);
      continue;
    }
    if (apply) {
      console.log('remapKey 0x' + fromKey.toString(16) +
                  '>0x' + toKey.toString(16));
      this.plugin_.remapKey(fromKey, toKey);
    } else {
      console.log('cancel remapKey 0x' + fromKey.toString(16));
      this.plugin_.remapKey(fromKey, fromKey);
    }
  }
}

/**
 * Set the shrink-to-fit and resize-to-client flags and save them if this is
 * a Me2Me connection.
 *
 * @param {boolean} shrinkToFit True if the remote desktop should be scaled
 *     down if it is larger than the client window; false if scroll-bars
 *     should be added in this case.
 * @param {boolean} resizeToClient True if window resizes should cause the
 *     host to attempt to resize its desktop to match the client window size;
 *     false to disable this behaviour for subsequent window resizes--the
 *     current host desktop size is not restored in this case.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.setScreenMode =
    function(shrinkToFit, resizeToClient) {
  if (resizeToClient && !this.resizeToClient_) {
    var clientArea = this.getClientArea_();
    this.plugin_.notifyClientResolution(clientArea.width,
                                        clientArea.height,
                                        window.devicePixelRatio);
  }

  // If enabling shrink, reset bump-scroll offsets.
  var needsScrollReset = shrinkToFit && !this.shrinkToFit_;

  this.shrinkToFit_ = shrinkToFit;
  this.resizeToClient_ = resizeToClient;
  this.updateScrollbarVisibility();

  if (this.hostId_ != '') {
    var options = {};
    options[remoting.ClientSession.KEY_SHRINK_TO_FIT] = this.shrinkToFit_;
    options[remoting.ClientSession.KEY_RESIZE_TO_CLIENT] = this.resizeToClient_;
    remoting.HostSettings.save(this.hostId_, options);
  }

  this.updateDimensions();
  if (needsScrollReset) {
    this.resetScroll_();
  }

}

/**
 * Called when the client receives its first frame.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onFirstFrameReceived = function() {
  this.hasReceivedFrame_ = true;
};

/**
 * @return {boolean} Whether the client has received a video buffer.
 */
remoting.ClientSession.prototype.hasReceivedFrame = function() {
  return this.hasReceivedFrame_;
};

/**
 * Sends an IQ stanza via the http xmpp proxy.
 *
 * @private
 * @param {string} msg XML string of IQ stanza to send to server.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendIq_ = function(msg) {
  // Extract the session id, so we can close the session later.
  var parser = new DOMParser();
  var iqNode = parser.parseFromString(msg, 'text/xml').firstChild;
  var jingleNode = iqNode.firstChild;
  if (jingleNode) {
    var action = jingleNode.getAttribute('action');
    if (jingleNode.nodeName == 'jingle' && action == 'session-initiate') {
      this.sessionId_ = jingleNode.getAttribute('sid');
    }
  }

  // HACK: Add 'x' prefix to the IDs of the outgoing messages to make sure that
  // stanza IDs used by host and client do not match. This is necessary to
  // workaround bug in the signaling endpoint used by chromoting.
  // TODO(sergeyu): Remove this hack once the server-side bug is fixed.
  var type = iqNode.getAttribute('type');
  if (type == 'set') {
    var id = iqNode.getAttribute('id');
    iqNode.setAttribute('id', 'x' + id);
    msg = (new XMLSerializer()).serializeToString(iqNode);
  }

  console.log(remoting.timestamp(), remoting.formatIq.prettifySendIq(msg));

  // Send the stanza.
  remoting.wcsSandbox.sendIq(msg);
};

remoting.ClientSession.prototype.initiateConnection_ = function() {
  /** @type {remoting.ClientSession} */
  var that = this;

  remoting.wcsSandbox.connect(onWcsConnected, this.resetWithError_.bind(this));

  /** @param {string} localJid Local JID. */
  function onWcsConnected(localJid) {
    that.connectPluginToWcs_(localJid);
    that.getSharedSecret_(onSharedSecretReceived.bind(null, localJid));
  }

  /** @param {string} localJid Local JID.
    * @param {string} sharedSecret Shared secret. */
  function onSharedSecretReceived(localJid, sharedSecret) {
    that.plugin_.connect(
        that.hostJid_, that.hostPublicKey_, localJid, sharedSecret,
        that.authenticationMethods_, that.hostId_, that.clientPairingId_,
        that.clientPairedSecret_);
  };
}

/**
 * Connects the plugin to WCS.
 *
 * @private
 * @param {string} localJid Local JID.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.connectPluginToWcs_ = function(localJid) {
  remoting.formatIq.setJids(localJid, this.hostJid_);
  var forwardIq = this.plugin_.onIncomingIq.bind(this.plugin_);
  /** @param {string} stanza The IQ stanza received. */
  var onIncomingIq = function(stanza) {
    // HACK: Remove 'x' prefix added to the id in sendIq_().
    try {
      var parser = new DOMParser();
      var iqNode = parser.parseFromString(stanza, 'text/xml').firstChild;
      var type = iqNode.getAttribute('type');
      var id = iqNode.getAttribute('id');
      if (type != 'set' && id.charAt(0) == 'x') {
        iqNode.setAttribute('id', id.substr(1));
        stanza = (new XMLSerializer()).serializeToString(iqNode);
      }
    } catch (err) {
      // Pass message as is when it is malformed.
    }

    console.log(remoting.timestamp(),
                remoting.formatIq.prettifyReceiveIq(stanza));
    forwardIq(stanza);
  };
  remoting.wcsSandbox.setOnIq(onIncomingIq);
}

/**
 * Gets shared secret to be used for connection.
 *
 * @param {function(string)} callback Callback called with the shared secret.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.getSharedSecret_ = function(callback) {
  /** @type remoting.ClientSession */
  var that = this;
  if (this.plugin_.hasFeature(remoting.ClientPlugin.Feature.THIRD_PARTY_AUTH)) {
    /** @type{function(string, string, string): void} */
    var fetchThirdPartyToken = function(tokenUrl, hostPublicKey, scope) {
      that.fetchThirdPartyToken_(
          tokenUrl, hostPublicKey, scope,
          that.plugin_.onThirdPartyTokenFetched.bind(that.plugin_));
    };
    this.plugin_.fetchThirdPartyTokenHandler = fetchThirdPartyToken;
  }
  if (this.accessCode_) {
    // Shared secret was already supplied before connecting (It2Me case).
    callback(this.accessCode_);
  } else if (this.plugin_.hasFeature(
      remoting.ClientPlugin.Feature.ASYNC_PIN)) {
    // Plugin supports asynchronously asking for the PIN.
    this.plugin_.useAsyncPinDialog();
    /** @param {boolean} pairingSupported */
    var fetchPin = function(pairingSupported) {
      that.fetchPin_(pairingSupported,
                     that.plugin_.onPinFetched.bind(that.plugin_));
    };
    this.plugin_.fetchPinHandler = fetchPin;
    callback('');
  } else {
    // Clients that don't support asking for a PIN asynchronously also don't
    // support pairing, so request the PIN now without offering to remember it.
    this.fetchPin_(false, callback);
  }
};

/**
 * Callback that the plugin invokes to indicate that the connection
 * status has changed.
 *
 * @private
 * @param {number} status The plugin's status.
 * @param {number} error The plugin's error state, if any.
 */
remoting.ClientSession.prototype.onConnectionStatusUpdate_ =
    function(status, error) {
  if (status == remoting.ClientSession.State.CONNECTED) {
    this.setFocusHandlers_();
    this.onDesktopSizeChanged_();
    if (this.resizeToClient_) {
      var clientArea = this.getClientArea_();
      this.plugin_.notifyClientResolution(clientArea.width,
                                          clientArea.height,
                                          window.devicePixelRatio);
    }
    // Activate full-screen related UX.
    remoting.fullscreen.addListener(this.callOnFullScreenChanged_);
    remoting.fullscreen.syncWithMaximize(true);
    if (remoting.windowFrame) {
      remoting.windowFrame.setClientSession(this);
    } else {
      remoting.toolbar.setClientSession(this);
    }

    this.container_.addEventListener('mousemove',
                                     this.updateMouseCursorPosition_,
                                     true);

  } else if (status == remoting.ClientSession.State.FAILED) {
    switch (error) {
      case remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE:
        this.error_ = remoting.Error.HOST_IS_OFFLINE;
        break;
      case remoting.ClientSession.ConnectionError.SESSION_REJECTED:
        this.error_ = remoting.Error.INVALID_ACCESS_CODE;
        break;
      case remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL:
        this.error_ = remoting.Error.INCOMPATIBLE_PROTOCOL;
        break;
      case remoting.ClientSession.ConnectionError.NETWORK_FAILURE:
        this.error_ = remoting.Error.P2P_FAILURE;
        break;
      case remoting.ClientSession.ConnectionError.HOST_OVERLOAD:
        this.error_ = remoting.Error.HOST_OVERLOAD;
        break;
      default:
        this.error_ = remoting.Error.UNEXPECTED;
    }
  }
  this.setState_(/** @type {remoting.ClientSession.State} */ (status));
};

/**
 * Callback that the plugin invokes to indicate when the connection is
 * ready.
 *
 * @private
 * @param {boolean} ready True if the connection is ready.
 */
remoting.ClientSession.prototype.onConnectionReady_ = function(ready) {
  if (!ready) {
    this.container_.classList.add('session-client-inactive');
  } else {
    this.container_.classList.remove('session-client-inactive');
  }

  this.raiseEvent(remoting.ClientSession.Events.videoChannelStateChanged,
                  ready);
};

/**
 * Called when the client-host capabilities negotiation is complete.
 *
 * @param {!Array.<string>} capabilities The set of capabilities negotiated
 *     between the client and host.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.onSetCapabilities_ = function(capabilities) {
  if (this.capabilities_ != null) {
    console.error('onSetCapabilities_() is called more than once');
    return;
  }

  this.capabilities_ = capabilities;
  if (this.hasCapability_(
      remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION)) {
    var clientArea = this.getClientArea_();
    this.plugin_.notifyClientResolution(clientArea.width,
                                        clientArea.height,
                                        window.devicePixelRatio);
  }
};

/**
 * @private
 * @param {remoting.ClientSession.State} newState The new state for the session.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.setState_ = function(newState) {
  var oldState = this.state_;
  this.state_ = newState;
  var state = this.state_;
  if (oldState == remoting.ClientSession.State.CONNECTING) {
    if (this.state_ == remoting.ClientSession.State.CLOSED) {
      state = remoting.ClientSession.State.CONNECTION_CANCELED;
    } else if (this.state_ == remoting.ClientSession.State.FAILED &&
        this.error_ == remoting.Error.HOST_IS_OFFLINE &&
        !this.logHostOfflineErrors_) {
      // The application requested host-offline errors to be suppressed, for
      // example, because this connection attempt is using a cached host JID.
      console.log('Suppressing host-offline error.');
      state = remoting.ClientSession.State.CONNECTION_CANCELED;
    }
  } else if (oldState == remoting.ClientSession.State.CONNECTED &&
             this.state_ == remoting.ClientSession.State.FAILED) {
    state = remoting.ClientSession.State.CONNECTION_DROPPED;
  }
  this.logToServer.logClientSessionStateChange(state, this.error_, this.mode_);
  if (this.state_ == remoting.ClientSession.State.CONNECTED) {
    this.createGnubbyAuthHandler_();
  }

  this.raiseEvent(remoting.ClientSession.Events.stateChanged,
    new remoting.ClientSession.StateEvent(newState, oldState)
  );
};

/**
 * This is a callback that gets called when the window is resized.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onResize = function() {
  this.updateDimensions();

  if (this.notifyClientResolutionTimer_) {
    window.clearTimeout(this.notifyClientResolutionTimer_);
    this.notifyClientResolutionTimer_ = null;
  }

  // Defer notifying the host of the change until the window stops resizing, to
  // avoid overloading the control channel with notifications.
  if (this.resizeToClient_) {
    var kResizeRateLimitMs = 1000;
    if (this.hasCapability_(
        remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS)) {
      kResizeRateLimitMs = 250;
    }
    var clientArea = this.getClientArea_();
    this.notifyClientResolutionTimer_ = window.setTimeout(
        this.plugin_.notifyClientResolution.bind(this.plugin_,
                                                 clientArea.width,
                                                 clientArea.height,
                                                 window.devicePixelRatio),
        kResizeRateLimitMs);
  }

  // If bump-scrolling is enabled, adjust the plugin margins to fully utilize
  // the new window area.
  this.resetScroll_();

  this.updateScrollbarVisibility();
};

/**
 * Requests that the host pause or resume video updates.
 *
 * @param {boolean} pause True to pause video, false to resume.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.pauseVideo = function(pause) {
  if (this.plugin_) {
    this.plugin_.pauseVideo(pause);
  }
};

/**
 * Requests that the host pause or resume audio.
 *
 * @param {boolean} pause True to pause audio, false to resume.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.pauseAudio = function(pause) {
  if (this.plugin_) {
    this.plugin_.pauseAudio(pause)
  }
}

/**
 * This is a callback that gets called when the plugin notifies us of a change
 * in the size of the remote desktop.
 *
 * @private
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onDesktopSizeChanged_ = function() {
  console.log('desktop size changed: ' +
              this.plugin_.desktopWidth + 'x' +
              this.plugin_.desktopHeight +' @ ' +
              this.plugin_.desktopXDpi + 'x' +
              this.plugin_.desktopYDpi + ' DPI');
  this.updateDimensions();
  this.updateScrollbarVisibility();
};

/**
 * Refreshes the plugin's dimensions, taking into account the sizes of the
 * remote desktop and client window, and the current scale-to-fit setting.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.updateDimensions = function() {
  if (this.plugin_.desktopWidth == 0 ||
      this.plugin_.desktopHeight == 0) {
    return;
  }

  var clientArea = this.getClientArea_();
  var desktopWidth = this.plugin_.desktopWidth;
  var desktopHeight = this.plugin_.desktopHeight;

  // When configured to display a host at its original size, we aim to display
  // it as close to its physical size as possible, without losing data:
  // - If client and host have matching DPI, render the host pixel-for-pixel.
  // - If the host has higher DPI then still render pixel-for-pixel.
  // - If the host has lower DPI then let Chrome up-scale it to natural size.

  // We specify the plugin dimensions in Density-Independent Pixels, so to
  // render pixel-for-pixel we need to down-scale the host dimensions by the
  // devicePixelRatio of the client. To match the host pixel density, we choose
  // an initial scale factor based on the client devicePixelRatio and host DPI.

  // Determine the effective device pixel ratio of the host, based on DPI.
  var hostPixelRatioX = Math.ceil(this.plugin_.desktopXDpi / 96);
  var hostPixelRatioY = Math.ceil(this.plugin_.desktopYDpi / 96);
  var hostPixelRatio = Math.min(hostPixelRatioX, hostPixelRatioY);

  // Down-scale by the smaller of the client and host ratios.
  var scale = 1.0 / Math.min(window.devicePixelRatio, hostPixelRatio);

  if (this.shrinkToFit_) {
    // Reduce the scale, if necessary, to fit the whole desktop in the window.
    var scaleFitWidth = Math.min(scale, 1.0 * clientArea.width / desktopWidth);
    var scaleFitHeight =
        Math.min(scale, 1.0 * clientArea.height / desktopHeight);
    scale = Math.min(scaleFitHeight, scaleFitWidth);

    // If we're running full-screen then try to handle common side-by-side
    // multi-monitor combinations more intelligently.
    if (remoting.fullscreen.isActive()) {
      // If the host has two monitors each the same size as the client then
      // scale-to-fit will have the desktop occupy only 50% of the client area,
      // in which case it would be preferable to down-scale less and let the
      // user bump-scroll around ("scale-and-pan").
      // Triggering scale-and-pan if less than 65% of the client area would be
      // used adds enough fuzz to cope with e.g. 1280x800 client connecting to
      // a (2x1280)x1024 host nicely.
      // Note that we don't need to account for scrollbars while fullscreen.
      if (scale <= scaleFitHeight * 0.65) {
        scale = scaleFitHeight;
      }
      if (scale <= scaleFitWidth * 0.65) {
        scale = scaleFitWidth;
      }
    }
  }

  var pluginWidth = Math.round(desktopWidth * scale);
  var pluginHeight = Math.round(desktopHeight * scale);

  if (this.video_) {
    this.video_.style.width = pluginWidth + 'px';
    this.video_.style.height = pluginHeight + 'px';
  }

  // Resize the plugin if necessary.
  // TODO(wez): Handle high-DPI to high-DPI properly (crbug.com/135089).
  this.plugin_.element().style.width = pluginWidth + 'px';
  this.plugin_.element().style.height = pluginHeight + 'px';

  // Position the container.
  // Note that clientWidth/Height take into account scrollbars.
  var clientWidth = document.documentElement.clientWidth;
  var clientHeight = document.documentElement.clientHeight;
  var parentNode = this.plugin_.element().parentNode;

  console.log('plugin dimensions: ' +
              parentNode.style.left + ',' +
              parentNode.style.top + '-' +
              pluginWidth + 'x' + pluginHeight + '.');
};

/**
 * Returns an associative array with a set of stats for this connection.
 *
 * @return {remoting.ClientSession.PerfStats} The connection statistics.
 */
remoting.ClientSession.prototype.getPerfStats = function() {
  return this.plugin_.getPerfStats();
};

/**
 * Logs statistics.
 *
 * @param {remoting.ClientSession.PerfStats} stats
 */
remoting.ClientSession.prototype.logStatistics = function(stats) {
  this.logToServer.logStatistics(stats, this.mode_);
};

/**
 * Enable or disable logging of connection errors due to a host being offline.
 * For example, if attempting a connection using a cached JID, host-offline
 * errors should not be logged because the JID will be refreshed and the
 * connection retried.
 *
 * @param {boolean} enable True to log host-offline errors; false to suppress.
 */
remoting.ClientSession.prototype.logHostOfflineErrors = function(enable) {
  this.logHostOfflineErrors_ = enable;
};

/**
 * Request pairing with the host for PIN-less authentication.
 *
 * @param {string} clientName The human-readable name of the client.
 * @param {function(string, string):void} onDone Callback to receive the
 *     client id and shared secret when they are available.
 */
remoting.ClientSession.prototype.requestPairing = function(clientName, onDone) {
  if (this.plugin_) {
    this.plugin_.requestPairing(clientName, onDone);
  }
};

/**
 * Called when the full-screen status has changed, either via the
 * remoting.Fullscreen class, or via a system event such as the Escape key
 *
 * @param {boolean} fullscreen True if the app is entering full-screen mode;
 *     false if it is leaving it.
 * @private
 */
remoting.ClientSession.prototype.onFullScreenChanged_ = function (fullscreen) {
  var htmlNode = /** @type {HTMLElement} */ (document.documentElement);
  this.enableBumpScroll_(fullscreen);
  if (fullscreen) {
    htmlNode.classList.add('full-screen');
  } else {
    htmlNode.classList.remove('full-screen');
  }
};

/**
 * Updates the options menu to reflect the current scale-to-fit and full-screen
 * settings.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.onShowOptionsMenu_ = function() {
  remoting.MenuButton.select(this.resizeToClientButton_, this.resizeToClient_);
  remoting.MenuButton.select(this.shrinkToFitButton_, this.shrinkToFit_);
  remoting.MenuButton.select(this.fullScreenButton_,
                             remoting.fullscreen.isActive());
};

/**
 * Scroll the client plugin by the specified amount, keeping it visible.
 * Note that this is only used in content full-screen mode (not windowed or
 * browser full-screen modes), where window.scrollBy and the scrollTop and
 * scrollLeft properties don't work.
 * @param {number} dx The amount by which to scroll horizontally. Positive to
 *     scroll right; negative to scroll left.
 * @param {number} dy The amount by which to scroll vertically. Positive to
 *     scroll down; negative to scroll up.
 * @return {boolean} True if the requested scroll had no effect because both
 *     vertical and horizontal edges of the screen have been reached.
 * @private
 */
remoting.ClientSession.prototype.scroll_ = function(dx, dy) {
  var plugin = this.plugin_.element();
  var style = plugin.style;

  /**
   * Helper function for x- and y-scrolling
   * @param {number|string} curr The current margin, eg. "10px".
   * @param {number} delta The requested scroll amount.
   * @param {number} windowBound The size of the window, in pixels.
   * @param {number} pluginBound The size of the plugin, in pixels.
   * @param {{stop: boolean}} stop Reference parameter used to indicate when
   *     the scroll has reached one of the edges and can be stopped in that
   *     direction.
   * @return {string} The new margin value.
   */
  var adjustMargin = function(curr, delta, windowBound, pluginBound, stop) {
    var minMargin = Math.min(0, windowBound - pluginBound);
    var result = (curr ? parseFloat(curr) : 0) - delta;
    result = Math.min(0, Math.max(minMargin, result));
    stop.stop = (result == 0 || result == minMargin);
    return result + 'px';
  };

  var stopX = { stop: false };
  var clientArea = this.getClientArea_();
  style.marginLeft = adjustMargin(style.marginLeft, dx,
                                  clientArea.width, plugin.clientWidth, stopX);

  var stopY = { stop: false };
  style.marginTop = adjustMargin(
      style.marginTop, dy, clientArea.height, plugin.clientHeight, stopY);
  return stopX.stop && stopY.stop;
};

remoting.ClientSession.prototype.resetScroll_ = function() {
  if (this.plugin_) {
    var plugin = this.plugin_.element();
    plugin.style.marginTop = '0px';
    plugin.style.marginLeft = '0px';
  }
};

/**
 * Enable or disable bump-scrolling. When disabling bump scrolling, also reset
 * the scroll offsets to (0, 0).
 * @private
 * @param {boolean} enable True to enable bump-scrolling, false to disable it.
 */
remoting.ClientSession.prototype.enableBumpScroll_ = function(enable) {
  var element = /*@type{HTMLElement} */ document.documentElement;
  if (enable) {
    /** @type {null|function(Event):void} */
    this.onMouseMoveRef_ = this.onMouseMove_.bind(this);
    element.addEventListener('mousemove', this.onMouseMoveRef_, false);
  } else {
    element.removeEventListener('mousemove', this.onMouseMoveRef_, false);
    this.onMouseMoveRef_ = null;
    this.resetScroll_();
  }
};

/**
 * @param {Event} event The mouse event.
 * @private
 */
remoting.ClientSession.prototype.onMouseMove_ = function(event) {
  if (this.bumpScrollTimer_) {
    window.clearTimeout(this.bumpScrollTimer_);
    this.bumpScrollTimer_ = null;
  }

  /**
   * Compute the scroll speed based on how close the mouse is to the edge.
   * @param {number} mousePos The mouse x- or y-coordinate
   * @param {number} size The width or height of the content area.
   * @return {number} The scroll delta, in pixels.
   */
  var computeDelta = function(mousePos, size) {
    var threshold = 10;
    if (mousePos >= size - threshold) {
      return 1 + 5 * (mousePos - (size - threshold)) / threshold;
    } else if (mousePos <= threshold) {
      return -1 - 5 * (threshold - mousePos) / threshold;
    }
    return 0;
  };

  var clientArea = this.getClientArea_();
  var dx = computeDelta(event.x, clientArea.width);
  var dy = computeDelta(event.y, clientArea.height);

  if (dx != 0 || dy != 0) {
    /** @type {remoting.ClientSession} */
    var that = this;
    /**
     * Scroll the view, and schedule a timer to do so again unless we've hit
     * the edges of the screen. This timer is cancelled when the mouse moves.
     * @param {number} expected The time at which we expect to be called.
     */
    var repeatScroll = function(expected) {
      /** @type {number} */
      var now = new Date().getTime();
      /** @type {number} */
      var timeout = 10;
      var lateAdjustment = 1 + (now - expected) / timeout;
      if (!that.scroll_(lateAdjustment * dx, lateAdjustment * dy)) {
        that.bumpScrollTimer_ = window.setTimeout(
            function() { repeatScroll(now + timeout); },
            timeout);
      }
    };
    repeatScroll(new Date().getTime());
  }
};

/**
 * Sends a clipboard item to the host.
 *
 * @param {string} mimeType The MIME type of the clipboard item.
 * @param {string} item The clipboard item.
 */
remoting.ClientSession.prototype.sendClipboardItem = function(mimeType, item) {
  if (!this.plugin_)
    return;
  this.plugin_.sendClipboardItem(mimeType, item);
};

/**
 * Send a gnubby-auth extension message to the host.
 * @param {Object} data The gnubby-auth message data.
 */
remoting.ClientSession.prototype.sendGnubbyAuthMessage = function(data) {
  if (!this.plugin_)
    return;
  this.plugin_.sendClientMessage('gnubby-auth', JSON.stringify(data));
};

/**
 * Process a remote gnubby auth request.
 * @param {string} data Remote gnubby request data.
 * @private
 */
remoting.ClientSession.prototype.processGnubbyAuthMessage_ = function(data) {
  if (this.gnubbyAuthHandler_) {
    try {
      this.gnubbyAuthHandler_.onMessage(data);
    } catch (err) {
      console.error('Failed to process gnubby message: ',
          /** @type {*} */ (err));
    }
  } else {
    console.error('Received unexpected gnubby message');
  }
};

/**
 * Create a gnubby auth handler and inform the host that gnubby auth is
 * supported.
 * @private
 */
remoting.ClientSession.prototype.createGnubbyAuthHandler_ = function() {
  if (this.mode_ == remoting.ClientSession.Mode.ME2ME) {
    this.gnubbyAuthHandler_ = new remoting.GnubbyAuthHandler(this);
    // TODO(psj): Move to more generic capabilities mechanism.
    this.sendGnubbyAuthMessage({'type': 'control', 'option': 'auth-v1'});
  }
};

/**
 * @return {{width: number, height: number}} The height of the window's client
 *     area. This differs between apps v1 and apps v2 due to the custom window
 *     borders used by the latter.
 * @private
 */
remoting.ClientSession.prototype.getClientArea_ = function() {
  return remoting.windowFrame ?
      remoting.windowFrame.getClientArea() :
      { 'width': window.innerWidth, 'height': window.innerHeight };
};

/**
 * @param {string} url
 * @param {number} hotspotX
 * @param {number} hotspotY
 */
remoting.ClientSession.prototype.updateMouseCursorImage_ =
    function(url, hotspotX, hotspotY) {
  this.mouseCursorOverlay_.hidden = !url;
  if (url) {
    this.mouseCursorOverlay_.style.marginLeft = '-' + hotspotX + 'px';
    this.mouseCursorOverlay_.style.marginTop = '-' + hotspotY + 'px';
    this.mouseCursorOverlay_.src = url;
  }
 };
