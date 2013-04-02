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
 * @param {string} hostJid The jid of the host to connect to.
 * @param {string} clientJid The jid of the WCS client.
 * @param {string} hostPublicKey The base64 encoded version of the host's
 *     public key.
 * @param {string} accessCode The IT2Me access code. Blank for Me2Me.
 * @param {function(function(string): void): void} fetchPin Called by Me2Me
 *     connections when a PIN needs to be obtained interactively.
 * @param {string} authenticationMethods Comma-separated list of
 *     authentication methods the client should attempt to use.
 * @param {string} hostId The host identifier for Me2Me, or empty for IT2Me.
 *     Mixed into authentication hashes for some authentication methods.
 * @param {remoting.ClientSession.Mode} mode The mode of this connection.
 * @param {string} hostDisplayName The name of the host for display purposes.
 * @constructor
 */
remoting.ClientSession = function(hostJid, clientJid, hostPublicKey, accessCode,
                                  fetchPin, authenticationMethods, hostId,
                                  mode, hostDisplayName) {
  this.state = remoting.ClientSession.State.CREATED;

  this.hostJid = hostJid;
  this.clientJid = clientJid;
  this.hostPublicKey = hostPublicKey;
  /** @private */
  this.accessCode_ = accessCode;
  /** @private */
  this.fetchPin_ = fetchPin;
  this.authenticationMethods = authenticationMethods;
  this.hostId = hostId;
  /** @type {string} */
  this.hostDisplayName = hostDisplayName;
  /** @type {remoting.ClientSession.Mode} */
  this.mode = mode;
  this.sessionId = '';
  /** @type {remoting.ClientPlugin} */
  this.plugin = null;
  /** @private */
  this.shrinkToFit_ = true;
  /** @private */
  this.resizeToClient_ = false;
  /** @private */
  this.hasReceivedFrame_ = false;
  this.logToServer = new remoting.LogToServer();
  /** @type {?function(remoting.ClientSession.State,
                       remoting.ClientSession.State):void} */
  this.onStateChange_ = null;

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
  this.callSetScreenMode_ = this.onSetScreenMode_.bind(this);
  /** @private */
  this.callToggleFullScreen_ = this.toggleFullScreen_.bind(this);

  /** @private */
  this.screenOptionsMenu_ = new remoting.MenuButton(
      document.getElementById('screen-options-menu'),
      this.onShowOptionsMenu_.bind(this));
  /** @private */
  this.sendKeysMenu_ = new remoting.MenuButton(
      document.getElementById('send-keys-menu')
  );

  /** @type {HTMLElement} @private */
  this.resizeToClientButton_ =
      document.getElementById('screen-resize-to-client');
  /** @type {HTMLElement} @private */
  this.shrinkToFitButton_ = document.getElementById('screen-shrink-to-fit');
  /** @type {HTMLElement} @private */
  this.fullScreenButton_ = document.getElementById('toggle-full-screen');

  if (this.mode == remoting.ClientSession.Mode.IT2ME) {
    // Resize-to-client is not supported for IT2Me hosts.
    this.resizeToClientButton_.hidden = true;
  } else {
    this.resizeToClientButton_.hidden = false;
    this.resizeToClientButton_.addEventListener(
        'click', this.callSetScreenMode_, false);
  }

  this.shrinkToFitButton_.addEventListener(
      'click', this.callSetScreenMode_, false);
  this.fullScreenButton_.addEventListener(
      'click', this.callToggleFullScreen_, false);
};

/**
 * @param {?function(remoting.ClientSession.State,
                     remoting.ClientSession.State):void} onStateChange
 *     The callback to invoke when the session changes state.
 */
remoting.ClientSession.prototype.setOnStateChange = function(onStateChange) {
  this.onStateChange_ = onStateChange;
};

// Note that the positive values in both of these enums are copied directly
// from chromoting_scriptable_object.h and must be kept in sync. The negative
// values represent state transitions that occur within the web-app that have
// no corresponding plugin state transition.
/** @enum {number} */
remoting.ClientSession.State = {
  CONNECTION_CANCELED: -5,  // Connection closed (gracefully) before connecting.
  CONNECTION_DROPPED: -4,  // Succeeded, but subsequently closed with an error.
  CREATED: -3,
  BAD_PLUGIN_VERSION: -2,
  UNKNOWN_PLUGIN_ERROR: -1,
  UNKNOWN: 0,
  CONNECTING: 1,
  INITIALIZING: 2,
  CONNECTED: 3,
  CLOSED: 4,
  FAILED: 5
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
remoting.ClientSession.KEY_RESIZE_TO_CLIENT = 'resizeToClient';
remoting.ClientSession.KEY_SHRINK_TO_FIT = 'shrinkToFit';

/**
 * The current state of the session.
 * @type {remoting.ClientSession.State}
 */
remoting.ClientSession.prototype.state = remoting.ClientSession.State.UNKNOWN;

/**
 * The last connection error. Set when state is set to FAILED.
 * @type {remoting.ClientSession.ConnectionError}
 * @private
 */
remoting.ClientSession.prototype.error_ =
    remoting.ClientSession.ConnectionError.NONE;

/**
 * The id of the client plugin
 *
 * @const
 */
remoting.ClientSession.prototype.PLUGIN_ID = 'session-client-plugin';

/**
 * @param {Element} container The element to add the plugin to.
 * @param {string} id Id to use for the plugin element .
 * @return {remoting.ClientPlugin} Create plugin object for the locally
 * installed plugin.
 */
remoting.ClientSession.prototype.createClientPlugin_ = function(container, id) {
  var plugin = /** @type {remoting.ViewerPlugin} */
      document.createElement('embed');

  plugin.id = id;
  plugin.src = 'about://none';
  plugin.type = 'application/vnd.chromium.remoting-viewer';
  plugin.width = 0;
  plugin.height = 0;
  plugin.tabIndex = 0;  // Required, otherwise focus() doesn't work.
  container.appendChild(plugin);

  return new remoting.ClientPluginAsync(plugin);
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
  if (this.plugin) {
    // Release all keys to prevent them becoming 'stuck down' on the host.
    this.plugin.releaseAllKeys();
    if (this.plugin.element()) {
      // Focus should stay on the element, not (for example) the toolbar.
      this.plugin.element().focus();
    }
  }
};

/**
 * Adds <embed> element to |container| and readies the sesion object.
 *
 * @param {Element} container The element to add the plugin to.
 */
remoting.ClientSession.prototype.createPluginAndConnect =
    function(container) {
  this.plugin = this.createClientPlugin_(container, this.PLUGIN_ID);
  remoting.HostSettings.load(this.hostId,
                             this.onHostSettingsLoaded_.bind(this));
};

/**
 * @param {Object.<string>} options The current options for the host, or {}
 *     if this client has no saved settings for the host.
 * @private
 */
remoting.ClientSession.prototype.onHostSettingsLoaded_ = function(options) {
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
  this.plugin.initialize(this.onPluginInitialized_.bind(this));
};

/**
 * Constrains the focus to the plugin element.
 * @private
 */
remoting.ClientSession.prototype.setFocusHandlers_ = function() {
  this.plugin.element().addEventListener(
      'focus', this.callPluginGotFocus_, false);
  this.plugin.element().addEventListener(
      'blur', this.callPluginLostFocus_, false);
  this.plugin.element().focus();
};

/**
 * @param {boolean} initialized
 */
remoting.ClientSession.prototype.onPluginInitialized_ = function(initialized) {
  if (!initialized) {
    console.error('ERROR: remoting plugin not loaded');
    this.plugin.cleanup();
    delete this.plugin;
    this.setState_(remoting.ClientSession.State.UNKNOWN_PLUGIN_ERROR);
    return;
  }

  if (!this.plugin.isSupportedVersion()) {
    this.plugin.cleanup();
    delete this.plugin;
    this.setState_(remoting.ClientSession.State.BAD_PLUGIN_VERSION);
    return;
  }

  // Show the Send Keys menu only if the plugin has the injectKeyEvent feature,
  // and the Ctrl-Alt-Del button only in Me2Me mode.
  if (!this.plugin.hasFeature(remoting.ClientPlugin.Feature.INJECT_KEY_EVENT)) {
    var sendKeysElement = document.getElementById('send-keys-menu');
    sendKeysElement.hidden = true;
  } else if (this.mode != remoting.ClientSession.Mode.ME2ME) {
    var sendCadElement = document.getElementById('send-ctrl-alt-del');
    sendCadElement.hidden = true;
  }

  // Remap the right Control key to the right Win / Cmd key on ChromeOS
  // platforms, if the plugin has the remapKey feature.
  if (this.plugin.hasFeature(remoting.ClientPlugin.Feature.REMAP_KEY) &&
      remoting.runningOnChromeOS()) {
    this.plugin.remapKey(0x0700e4, 0x0700e7);
  }

  /** @param {string} msg The IQ stanza to send. */
  this.plugin.onOutgoingIqHandler = this.sendIq_.bind(this);
  /** @param {string} msg The message to log. */
  this.plugin.onDebugMessageHandler = function(msg) {
    console.log('plugin: ' + msg);
  };

  this.plugin.onConnectionStatusUpdateHandler =
      this.onConnectionStatusUpdate_.bind(this);
  this.plugin.onConnectionReadyHandler =
      this.onConnectionReady_.bind(this);
  this.plugin.onDesktopSizeUpdateHandler =
      this.onDesktopSizeChanged_.bind(this);

  this.connectPluginToWcs_();
};

/**
 * Deletes the <embed> element from the container, without sending a
 * session_terminate request.  This is to be called when the session was
 * disconnected by the Host.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.removePlugin = function() {
  if (this.plugin) {
    this.plugin.element().removeEventListener(
        'focus', this.callPluginGotFocus_, false);
    this.plugin.element().removeEventListener(
        'blur', this.callPluginLostFocus_, false);
    this.plugin.cleanup();
    this.plugin = null;
  }
  this.resizeToClientButton_.removeEventListener(
      'click', this.callSetScreenMode_, false);
  this.shrinkToFitButton_.removeEventListener(
      'click', this.callSetScreenMode_, false);
  this.fullScreenButton_.removeEventListener(
      'click', this.callToggleFullScreen_, false);
};

/**
 * Deletes the <embed> element from the container and disconnects.
 *
 * @param {boolean} isUserInitiated True for user-initiated disconnects, False
 *     for disconnects due to connection failures.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.disconnect = function(isUserInitiated) {
  if (isUserInitiated) {
    // The plugin won't send a state change notification, so we explicitly log
    // the fact that the connection has closed.
    this.logToServer.logClientSessionStateChange(
        remoting.ClientSession.State.CLOSED,
        remoting.ClientSession.ConnectionError.NONE, this.mode);
  }
  remoting.wcsSandbox.setOnIq(null);
  this.sendIq_(
      '<cli:iq ' +
          'to="' + this.hostJid + '" ' +
          'type="set" ' +
          'id="session-terminate" ' +
          'xmlns:cli="jabber:client">' +
        '<jingle ' +
            'xmlns="urn:xmpp:jingle:1" ' +
            'action="session-terminate" ' +
            'initiator="' + this.clientJid + '" ' +
            'sid="' + this.sessionId + '">' +
          '<reason><success/></reason>' +
        '</jingle>' +
      '</cli:iq>');
  this.removePlugin();
};

/**
 * @return {?remoting.Error} The current error code, or null if the connection
 *     is not in an error state.
 */
remoting.ClientSession.prototype.getError = function() {
  switch (this.error_) {
    case remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE:
      return remoting.Error.HOST_IS_OFFLINE;
    case remoting.ClientSession.ConnectionError.SESSION_REJECTED:
      return remoting.Error.INVALID_ACCESS_CODE;
    case remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL:
      return remoting.Error.INCOMPATIBLE_PROTOCOL;
    case remoting.ClientSession.ConnectionError.NETWORK_FAILURE:
      return remoting.Error.P2P_FAILURE;
    case remoting.ClientSession.ConnectionError.HOST_OVERLOAD:
      return remoting.Error.HOST_OVERLOAD;
    default:
      return null;
  }
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
    this.plugin.injectKeyEvent(keys[i], true);
  }
  for (var i = 0; i < keys.length; i++) {
    this.plugin.injectKeyEvent(keys[i], false);
  }
}

/**
 * Sends a Ctrl-Alt-Del sequence to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendCtrlAltDel = function() {
  this.sendKeyCombination_([0x0700e0, 0x0700e2, 0x07004c]);
}

/**
 * Sends a Print Screen keypress to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendPrintScreen = function() {
  this.sendKeyCombination_([0x070046]);
}

/**
 * Callback for the two "screen mode" related menu items: Resize desktop to
 * fit and Shrink to fit.
 *
 * @param {Event} event The click event indicating which mode was selected.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.onSetScreenMode_ = function(event) {
  var shrinkToFit = this.shrinkToFit_;
  var resizeToClient = this.resizeToClient_;
  if (event.target == this.shrinkToFitButton_) {
    shrinkToFit = !shrinkToFit;
  }
  if (event.target == this.resizeToClientButton_) {
    resizeToClient = !resizeToClient;
  }
  this.setScreenMode_(shrinkToFit, resizeToClient);
};

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
 * @private
 */
remoting.ClientSession.prototype.setScreenMode_ =
    function(shrinkToFit, resizeToClient) {

  if (resizeToClient && !this.resizeToClient_) {
    this.plugin.notifyClientResolution(window.innerWidth,
                                       window.innerHeight,
                                       window.devicePixelRatio);
  }

  // If enabling shrink, reset bump-scroll offsets.
  var needsScrollReset = shrinkToFit && !this.shrinkToFit_;

  this.shrinkToFit_ = shrinkToFit;
  this.resizeToClient_ = resizeToClient;

  if (this.hostId != '') {
    var options = {};
    options[remoting.ClientSession.KEY_SHRINK_TO_FIT] = this.shrinkToFit_;
    options[remoting.ClientSession.KEY_RESIZE_TO_CLIENT] = this.resizeToClient_;
    remoting.HostSettings.save(this.hostId, options);
  }

  this.updateDimensions();
  if (needsScrollReset) {
    this.scroll_(0, 0);
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
      this.sessionId = jingleNode.getAttribute('sid');
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

/**
 * Connects the plugin to WCS.
 *
 * @private
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.connectPluginToWcs_ = function() {
  remoting.formatIq.setJids(this.clientJid, this.hostJid);
  /** @type {remoting.ClientPlugin} */
  var plugin = this.plugin;
  var forwardIq = plugin.onIncomingIq.bind(plugin);
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

  if (this.accessCode_) {
    // Shared secret was already supplied before connecting (It2Me case).
    this.connectToHost_(this.accessCode_);

  } else if (plugin.hasFeature(
      remoting.ClientPlugin.Feature.ASYNC_PIN)) {
    // Plugin supports asynchronously asking for the PIN.
    plugin.useAsyncPinDialog();
    /** @type remoting.ClientSession */
    var that = this;
    var fetchPin = function() {
      that.fetchPin_(plugin.onPinFetched.bind(plugin));
    };
    plugin.fetchPinHandler = fetchPin;
    this.connectToHost_('');

  } else {
    // Plugin doesn't support asynchronously asking for the PIN, ask now.
    this.fetchPin_(this.connectToHost_.bind(this));
  }
};

/**
 * Connects to the host.
 *
 * @param {string} sharedSecret Shared secret for SPAKE negotiation.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.connectToHost_ = function(sharedSecret) {
  this.plugin.connect(this.hostJid, this.hostPublicKey, this.clientJid,
                      sharedSecret, this.authenticationMethods,
                      this.hostId);
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
      this.plugin.notifyClientResolution(window.innerWidth,
                                         window.innerHeight,
                                         window.devicePixelRatio);
    }
  } else if (status == remoting.ClientSession.State.FAILED) {
    this.error_ = /** @type {remoting.ClientSession.ConnectionError} */ (error);
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
    this.plugin.element().classList.add("session-client-inactive");
  } else {
    this.plugin.element().classList.remove("session-client-inactive");
  }
}

/**
 * @private
 * @param {remoting.ClientSession.State} newState The new state for the session.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.setState_ = function(newState) {
  var oldState = this.state;
  this.state = newState;
  var state = this.state;
  if (oldState == remoting.ClientSession.State.CONNECTING) {
    if (this.state == remoting.ClientSession.State.CLOSED) {
      state = remoting.ClientSession.State.CONNECTION_CANCELED;
    } else if (this.state == remoting.ClientSession.State.FAILED &&
        this.error_ == remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE &&
        !this.logHostOfflineErrors_) {
      // The application requested host-offline errors to be suppressed, for
      // example, because this connection attempt is using a cached host JID.
      console.log('Suppressing host-offline error.');
      state = remoting.ClientSession.State.CONNECTION_CANCELED;
    }
  } else if (oldState == remoting.ClientSession.State.CONNECTED &&
             this.state == remoting.ClientSession.State.FAILED) {
    state = remoting.ClientSession.State.CONNECTION_DROPPED;
  }
  this.logToServer.logClientSessionStateChange(state, this.error_, this.mode);
  if (this.onStateChange_) {
    this.onStateChange_(oldState, newState);
  }
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
    this.notifyClientResolutionTimer_ = window.setTimeout(
        this.plugin.notifyClientResolution.bind(this.plugin,
                                                window.innerWidth,
                                                window.innerHeight,
                                                window.devicePixelRatio),
        1000);
  }

  // If bump-scrolling is enabled, adjust the plugin margins to fully utilize
  // the new window area.
  this.scroll_(0, 0);
};

/**
 * Requests that the host pause or resume video updates.
 *
 * @param {boolean} pause True to pause video, false to resume.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.pauseVideo = function(pause) {
  if (this.plugin) {
    this.plugin.pauseVideo(pause)
  }
}

/**
 * Requests that the host pause or resume audio.
 *
 * @param {boolean} pause True to pause audio, false to resume.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.pauseAudio = function(pause) {
  if (this.plugin) {
    this.plugin.pauseAudio(pause)
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
              this.plugin.desktopWidth + 'x' +
              this.plugin.desktopHeight +' @ ' +
              this.plugin.desktopXDpi + 'x' +
              this.plugin.desktopYDpi + ' DPI');
  this.updateDimensions();
};

/**
 * Refreshes the plugin's dimensions, taking into account the sizes of the
 * remote desktop and client window, and the current scale-to-fit setting.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.updateDimensions = function() {
  if (this.plugin.desktopWidth == 0 ||
      this.plugin.desktopHeight == 0) {
    return;
  }

  var windowWidth = window.innerWidth;
  var windowHeight = window.innerHeight;
  var desktopWidth = this.plugin.desktopWidth;
  var desktopHeight = this.plugin.desktopHeight;

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
  var hostPixelRatioX = Math.ceil(this.plugin.desktopXDpi / 96);
  var hostPixelRatioY = Math.ceil(this.plugin.desktopYDpi / 96);
  var hostPixelRatio = Math.min(hostPixelRatioX, hostPixelRatioY);

  // Down-scale by the smaller of the client and host ratios.
  var scale = 1.0 / Math.min(window.devicePixelRatio, hostPixelRatio);

  if (this.shrinkToFit_) {
    // Reduce the scale, if necessary, to fit the whole desktop in the window.
    var scaleFitWidth = Math.min(scale, 1.0 * windowWidth / desktopWidth);
    var scaleFitHeight = Math.min(scale, 1.0 * windowHeight / desktopHeight);
    scale = Math.min(scaleFitHeight, scaleFitWidth);

    // If we're running full-screen then try to handle common side-by-side
    // multi-monitor combinations more intelligently.
    if (document.webkitIsFullScreen) {
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

  var pluginWidth = desktopWidth * scale;
  var pluginHeight = desktopHeight * scale;

  // Resize the plugin if necessary.
  // TODO(wez): Handle high-DPI to high-DPI properly (crbug.com/135089).
  this.plugin.element().width = pluginWidth;
  this.plugin.element().height = pluginHeight;

  // Position the container.
  // Note that clientWidth/Height take into account scrollbars.
  var clientWidth = document.documentElement.clientWidth;
  var clientHeight = document.documentElement.clientHeight;
  var parentNode = this.plugin.element().parentNode;

  if (pluginWidth < clientWidth) {
    parentNode.style.left = (clientWidth - pluginWidth) / 2 + 'px';
  } else {
    parentNode.style.left = '0';
  }

  if (pluginHeight < clientHeight) {
    parentNode.style.top = (clientHeight - pluginHeight) / 2 + 'px';
  } else {
    parentNode.style.top = '0';
  }

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
  return this.plugin.getPerfStats();
};

/**
 * Logs statistics.
 *
 * @param {remoting.ClientSession.PerfStats} stats
 */
remoting.ClientSession.prototype.logStatistics = function(stats) {
  this.logToServer.logStatistics(stats, this.mode);
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
 * Toggles between full-screen and windowed mode.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.toggleFullScreen_ = function() {
  if (document.webkitIsFullScreen) {
    document.webkitCancelFullScreen();
    this.enableBumpScroll_(false);
  } else {
    document.body.webkitRequestFullScreen(Element.ALLOW_KEYBOARD_INPUT);
    // Don't enable bump scrolling immediately because it can result in
    // onMouseMove firing before the webkitIsFullScreen property can be
    // read safely (crbug.com/132180).
    window.setTimeout(this.enableBumpScroll_.bind(this, true), 0);
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
      document.webkitIsFullScreen);
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
  var plugin = this.plugin.element();
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
    return result + "px";
  };

  var stopX = { stop: false };
  style.marginLeft = adjustMargin(style.marginLeft, dx,
                                  window.innerWidth, plugin.width, stopX);
  var stopY = { stop: false };
  style.marginTop = adjustMargin(style.marginTop, dy,
                                 window.innerHeight, plugin.height, stopY);
  return stopX.stop && stopY.stop;
}

/**
 * Enable or disable bump-scrolling. When disabling bump scrolling, also reset
 * the scroll offsets to (0, 0).
 * @private
 * @param {boolean} enable True to enable bump-scrolling, false to disable it.
 */
remoting.ClientSession.prototype.enableBumpScroll_ = function(enable) {
  if (enable) {
    /** @type {null|function(Event):void} */
    this.onMouseMoveRef_ = this.onMouseMove_.bind(this);
    this.plugin.element().addEventListener('mousemove', this.onMouseMoveRef_,
                                           false);
  } else {
    this.plugin.element().removeEventListener('mousemove', this.onMouseMoveRef_,
                                              false);
    this.onMouseMoveRef_ = null;
    this.plugin.element().style.marginLeft = 0;
    this.plugin.element().style.marginTop = 0;
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
  // It's possible to leave content full-screen mode without using the Screen
  // Options menu, so we disable bump scrolling as soon as we detect this.
  if (!document.webkitIsFullScreen) {
    this.enableBumpScroll_(false);
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

  var dx = computeDelta(event.x, window.innerWidth);
  var dy = computeDelta(event.y, window.innerHeight);

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
