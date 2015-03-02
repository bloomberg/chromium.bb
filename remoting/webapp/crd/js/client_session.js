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
 * Interval that determines how often the web-app should send a new access token
 * to the host.
 *
 * @const
 * @type {number}
 */
remoting.ACCESS_TOKEN_RESEND_INTERVAL_MS = 15 * 60 * 1000;

/**
 * @param {remoting.Host} host The host to connect to.
 * @param {remoting.SignalStrategy} signalStrategy Signal strategy.
 * @param {remoting.CredentialsProvider} credentialsProvider
 *     The credentialsProvider to authenticate the client with the host.
 * @param {HTMLElement} container Container element for the client view.
 * @param {remoting.DesktopConnectedView.Mode} mode The mode of this connection.
 * @param {string} defaultRemapKeys The default set of remap keys, to use
 *     when the client doesn't define any.
 * @constructor
 * @extends {base.EventSourceImpl}
 */
remoting.ClientSession = function(host, signalStrategy, credentialsProvider,
                                  container, mode, defaultRemapKeys) {
  /** @private */
  this.state_ = remoting.ClientSession.State.CREATED;

  /** @private */
  this.error_ = remoting.Error.NONE;

  /** @private */
  this.host_ = host;

  /** @private */
  this.credentialsProvider_ = credentialsProvider;

  /** @private */
  this.uiHandler_ = new remoting.DesktopConnectedView(
      this, container, this.host_, mode, defaultRemapKeys,
      this.onPluginInitialized_.bind(this));
  remoting.desktopConnectedView = this.uiHandler_;

  /** @private */
  this.sessionId_ = '';
  /** @type {remoting.ClientPlugin}
    * @private */
  this.plugin_ = null;
  /** @private */
  this.hasReceivedFrame_ = false;
  this.logToServer = new remoting.LogToServer(signalStrategy, mode);

  /** @private */
  this.signalStrategy_ = signalStrategy;
  base.debug.assert(this.signalStrategy_.getState() ==
                    remoting.SignalStrategy.State.CONNECTED);
  this.signalStrategy_.setIncomingStanzaCallback(
      this.onIncomingMessage_.bind(this));
  remoting.formatIq.setJids(this.signalStrategy_.getJid(), host.jabberId);

  /**
   * Allow host-offline error reporting to be suppressed in situations where it
   * would not be useful, for example, when using a cached host JID.
   *
   * @type {boolean} @private
   */
  this.logHostOfflineErrors_ = true;

  /** @type {remoting.GnubbyAuthHandler} @private */
  this.gnubbyAuthHandler_ = null;

  /** @type {remoting.CastExtensionHandler} @private */
  this.castExtensionHandler_ = null;

  this.defineEvents(Object.keys(remoting.ClientSession.Events));
};

base.extend(remoting.ClientSession, base.EventSourceImpl);

/** @enum {string} */
remoting.ClientSession.Events = {
  stateChanged: 'stateChanged',
  videoChannelStateChanged: 'videoChannelStateChanged',
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
  @param {remoting.ClientSession.State} current
  @param {remoting.ClientSession.State} previous
  @constructor
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

/**
 * Set of capabilities for which hasCapability() can be used to test.
 *
 * @enum {string}
 */
remoting.ClientSession.Capability = {
  // When enabled this capability causes the client to send its screen
  // resolution to the host once connection has been established. See
  // this.plugin_.notifyClientResolution().
  SEND_INITIAL_RESOLUTION: 'sendInitialResolution',

  // Let the host know that we're interested in knowing whether or not it
  // rate limits desktop-resize requests.
  // TODO(kelvinp): This has been supported since M-29.  Currently we only have
  // <1000 users on M-29 or below. Remove this and the capability on the host.
  RATE_LIMIT_RESIZE_REQUESTS: 'rateLimitResizeRequests',

  // Indicates that host/client supports Google Drive integration, and that the
  // client should send to the host the OAuth tokens to be used by Google Drive
  // on the host.
  GOOGLE_DRIVE: "googleDrive",

  // Indicates that the client supports the video frame-recording extension.
  VIDEO_RECORDER: 'videoRecorder',

  // Indicates that the client supports 'cast'ing the video stream to a
  // cast-enabled device.
  CAST: 'casting',
};

/**
 * The set of capabilities negotiated between the client and host.
 * @type {Array<string>}
 * @private
 */
remoting.ClientSession.prototype.capabilities_ = null;

/**
 * @param {remoting.ClientSession.Capability} capability The capability to test
 *     for.
 * @return {boolean} True if the capability has been negotiated between
 *     the client and host.
 */
remoting.ClientSession.prototype.hasCapability = function(capability) {
  if (this.capabilities_ == null)
    return false;

  return this.capabilities_.indexOf(capability) > -1;
};

/**
 * Adds <embed> element to the UI container and readies the session object.
 *
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @param {Array<string>} requiredCapabilities A list of capabilities
 *     required by this application.
 */
remoting.ClientSession.prototype.createPluginAndConnect =
    function(onExtensionMessage, requiredCapabilities) {
  this.uiHandler_.createPluginAndConnect(onExtensionMessage,
                                         requiredCapabilities);
};

/**
 * @param {remoting.Error} error
 * @param {remoting.ClientPlugin} plugin
 */
remoting.ClientSession.prototype.onPluginInitialized_ = function(
    error, plugin) {
  if (error != remoting.Error.NONE) {
    this.resetWithError_(error);
  }

  this.plugin_ = plugin;
  plugin.setOnOutgoingIqHandler(this.sendIq_.bind(this));
  plugin.setOnDebugMessageHandler(this.onDebugMessage_.bind(this));

  plugin.setConnectionStatusUpdateHandler(
      this.onConnectionStatusUpdate_.bind(this));
  plugin.setRouteChangedHandler(this.onRouteChanged_.bind(this));
  plugin.setConnectionReadyHandler(this.onConnectionReady_.bind(this));
  plugin.setCapabilitiesHandler(this.onSetCapabilities_.bind(this));
  plugin.setGnubbyAuthHandler(
      this.processGnubbyAuthMessage_.bind(this));
  plugin.setCastExtensionHandler(
      this.processCastExtensionMessage_.bind(this));

  this.plugin_.connect(
      this.host_, this.signalStrategy_.getJid(), this.credentialsProvider_);
};

/**
 * @param {remoting.Error} error
 */
remoting.ClientSession.prototype.resetWithError_ = function(error) {
  this.signalStrategy_.setIncomingStanzaCallback(null);
  this.removePlugin();
  this.error_ = error;
  this.setState_(remoting.ClientSession.State.FAILED);
};

/**
 * Deletes the <embed> element from the container, without sending a
 * session_terminate request.  This is to be called when the session was
 * disconnected by the Host.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.removePlugin = function() {
  this.uiHandler_.removePlugin();
  this.plugin_ = null;
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
  this.logToServer.logClientSessionStateChange(state, error);
  this.error_ = error;
  this.setState_(state);
};

/**
 * Deletes the <embed> element from the container and disconnects.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.cleanup = function() {
  this.sendIq_(
      '<cli:iq ' +
          'to="' + this.host_.jabberId + '" ' +
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
 * Sends a signaling message.
 *
 * @param {string} message XML string of IQ stanza to send to server.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.sendIq_ = function(message) {
  // Extract the session id, so we can close the session later.
  var parser = new DOMParser();
  var iqNode = parser.parseFromString(message, 'text/xml').firstChild;
  var jingleNode = iqNode.firstChild;
  if (jingleNode) {
    var action = jingleNode.getAttribute('action');
    if (jingleNode.nodeName == 'jingle' && action == 'session-initiate') {
      this.sessionId_ = jingleNode.getAttribute('sid');
    }
  }

  console.log(remoting.timestamp(), remoting.formatIq.prettifySendIq(message));
  if (this.signalStrategy_.getState() !=
      remoting.SignalStrategy.State.CONNECTED) {
    console.log("Message above is dropped because signaling is not connected.");
    return;
  }

  this.signalStrategy_.sendMessage(message);
};

/**
 * @param {string} msg
 * @private
 */
remoting.ClientSession.prototype.onDebugMessage_ = function(msg) {
  console.log('plugin: ' + msg.trimRight());
};

/**
 * @param {Element} message
 * @private
 */
remoting.ClientSession.prototype.onIncomingMessage_ = function(message) {
  if (!this.plugin_) {
    return;
  }
  var formatted = new XMLSerializer().serializeToString(message);
  console.log(remoting.timestamp(),
              remoting.formatIq.prettifyReceiveIq(formatted));
  this.plugin_.onIncomingIq(formatted);
};

/**
 * Callback that the plugin invokes to indicate that the connection
 * status has changed.
 *
 * @param {number} status The plugin's status.
 * @param {number} error The plugin's error state, if any.
 * @private
 */
remoting.ClientSession.prototype.onConnectionStatusUpdate_ =
    function(status, error) {
  if (status == remoting.ClientSession.State.CONNECTED) {
    this.uiHandler_.updateClientSessionUi_(this);

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
 * Callback that the plugin invokes to indicate that the connection type for
 * a channel has changed.
 *
 * @param {string} channel The channel name.
 * @param {string} connectionType The new connection type.
 * @private
 */
remoting.ClientSession.prototype.onRouteChanged_ =
    function(channel, connectionType) {
  console.log('plugin: Channel ' + channel + ' using ' +
              connectionType + ' connection.');
  this.logToServer.setConnectionType(connectionType);
};

/**
 * Callback that the plugin invokes to indicate when the connection is
 * ready.
 *
 * @param {boolean} ready True if the connection is ready.
 * @private
 */
remoting.ClientSession.prototype.onConnectionReady_ = function(ready) {
  // TODO(jamiewalch): Currently, the logic for determining whether or not the
  // connection is available is based solely on whether or not any video frames
  // have been received recently. which leads to poor UX on slow connections.
  // Re-enable this once crbug.com/435315 has been fixed.
  var ignoreVideoChannelState = true;
  if (ignoreVideoChannelState) {
    console.log('Video channel ' + (ready ? '' : 'not ') + 'ready.');
    return;
  }

  this.uiHandler_.onConnectionReady(ready);

  this.raiseEvent(remoting.ClientSession.Events.videoChannelStateChanged,
                  ready);
};

/**
 * Called when the client-host capabilities negotiation is complete.
 * TODO(kelvinp): Move this function out of ClientSession.
 *
 * @param {!Array<string>} capabilities The set of capabilities negotiated
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
  if (this.hasCapability(remoting.ClientSession.Capability.GOOGLE_DRIVE)) {
    this.sendGoogleDriveAccessToken_();
  }
  if (this.hasCapability(
      remoting.ClientSession.Capability.VIDEO_RECORDER)) {
    this.uiHandler_.initVideoFrameRecorder();
  }
};

/**
 * @param {remoting.ClientSession.State} newState The new state for the session.
 * @return {void} Nothing.
 * @private
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
  this.logToServer.logClientSessionStateChange(state, this.error_);
  if (this.state_ == remoting.ClientSession.State.CONNECTED) {
    this.createGnubbyAuthHandler_();
    this.createCastExtensionHandler_();
  }

  this.raiseEvent(remoting.ClientSession.Events.stateChanged,
    new remoting.ClientSession.StateEvent(newState, oldState)
  );
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
  this.logToServer.logStatistics(stats);
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
 * Sends an extension message to the host.
 *
 * @param {string} type The message type.
 * @param {string} message The message payload.
 */
remoting.ClientSession.prototype.sendClientMessage = function(type, message) {
  if (!this.plugin_)
    return;
  this.plugin_.sendClientMessage(type, message);
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
    } catch (/** @type {*} */ err) {
      console.error('Failed to process gnubby message: ', err);
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
  if (this.uiHandler_.getMode() == remoting.DesktopConnectedView.Mode.ME2ME) {
    this.gnubbyAuthHandler_ = new remoting.GnubbyAuthHandler(this);
    // TODO(psj): Move to more generic capabilities mechanism.
    this.sendGnubbyAuthMessage({'type': 'control', 'option': 'auth-v1'});
  }
};

/**
 * Timer callback to send the access token to the host.
 * @private
 */
remoting.ClientSession.prototype.sendGoogleDriveAccessToken_ = function() {
  if (this.state_ != remoting.ClientSession.State.CONNECTED) {
    return;
  }
  /** @type {remoting.ClientSession} */
  var that = this;

  /** @param {string} token */
  var sendToken = function(token) {
    remoting.clientSession.sendClientMessage('accessToken', token);
  };
  /** @param {remoting.Error} error */
  var sendError = function(error) {
    console.log('Failed to refresh access token: ' + error);
  };
  remoting.identity.getNewToken().
      then(sendToken).
      catch(remoting.Error.handler(sendError));
  window.setTimeout(this.sendGoogleDriveAccessToken_.bind(this),
                    remoting.ACCESS_TOKEN_RESEND_INTERVAL_MS);
};

/**
 * Send a Cast extension message to the host.
 * @param {Object} data The cast message data.
 */
remoting.ClientSession.prototype.sendCastExtensionMessage = function(data) {
  if (!this.plugin_)
    return;
  this.plugin_.sendClientMessage('cast_message', JSON.stringify(data));
};

/**
 * Process a remote Cast extension message from the host.
 * @param {string} data Remote cast extension data message.
 * @private
 */
remoting.ClientSession.prototype.processCastExtensionMessage_ = function(data) {
  if (this.castExtensionHandler_) {
    try {
      this.castExtensionHandler_.onMessage(data);
    } catch (/** @type {*} */ err) {
      console.error('Failed to process cast message: ', err);
    }
  } else {
    console.error('Received unexpected cast message');
  }
};

/**
 * Create a CastExtensionHandler and inform the host that cast extension
 * is supported.
 * @private
 */
remoting.ClientSession.prototype.createCastExtensionHandler_ = function() {
  if (remoting.app.hasCapability(remoting.ClientSession.Capability.CAST) &&
      this.uiHandler_.getMode() == remoting.DesktopConnectedView.Mode.ME2ME) {
    this.castExtensionHandler_ = new remoting.CastExtensionHandler(this);
  }
};

/**
 * Handles protocol extension messages.
 * @param {string} type Type of extension message.
 * @param {Object} message The parsed extension message data.
 * @return {boolean} True if the message was recognized, false otherwise.
 */
remoting.ClientSession.prototype.handleExtensionMessage =
    function(type, message) {
  if (this.uiHandler_.handleExtensionMessage(type, message)) {
    return true;
  }
  return false;
};

/**
 * Enables or disables rendering of dirty regions for debugging.
 * @param {boolean} enable True to enable rendering.
 */
remoting.ClientSession.prototype.enableDebugRegion = function(enable) {
  if (enable) {
    this.plugin_.setDebugDirtyRegionHandler(
        this.uiHandler_.handleDebugRegion.bind(this.uiHandler_));
  } else {
    this.plugin_.setDebugDirtyRegionHandler(null);
  }
}
