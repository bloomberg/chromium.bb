// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Session class that handles creation and teardown of a remoting session.
 *
 * This abstracts a <embed> element and controls the plugin which does the
 * actual remoting work.  There should be no UI code inside this class.  It
 * should be purely thought of as a controller of sorts.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {
/**
 * @param {string} hostJid The jid of the host to connect to.
 * @param {string} hostPublicKey The base64 encoded version of the host's
 *     public key.
 * @param {string} accessCode The access code for the IT2Me connection.
 * @param {string} email The username for the talk network.
 * @param {function(remoting.ClientSession.State):void} onStateChange
 *     The callback to invoke when the session changes state. This callback
 *     occurs after the state changes and is passed the previous state; the
 *     new state is accessible via ClientSession's |state| property.
 * @constructor
 */
remoting.ClientSession = function(hostJid, hostPublicKey, accessCode, email,
                                  onStateChange) {
  this.state = remoting.ClientSession.State.CREATED;

  this.hostJid = hostJid;
  this.hostPublicKey = hostPublicKey;
  this.accessCode = accessCode;
  this.email = email;
  this.clientJid = '';
  this.onStateChange = onStateChange;
};

/** @enum {number} */
remoting.ClientSession.State = {
  UNKNOWN: 0,
  CREATED: 1,
  BAD_PLUGIN_VERSION: 2,
  UNKNOWN_PLUGIN_ERROR: 3,
  CONNECTING: 4,
  INITIALIZING: 5,
  CONNECTED: 6,
  CLOSED: 7,
  CONNECTION_FAILED: 8
};

/**
 * The current state of the session.
 * @type {remoting.ClientSession.State}
 */
remoting.ClientSession.prototype.state = remoting.ClientSession.State.UNKNOWN;

/**
 * Chromoting session API version (for this javascript).
 * This is compared with the plugin API version to verify that they are
 * compatible.
 *
 * @const
 * @private
 */
remoting.ClientSession.prototype.API_VERSION_ = 2;

/**
 * Server used to bridge into the Jabber network for establishing Jingle
 * connections.
 *
 * @const
 * @private
 */
remoting.ClientSession.prototype.HTTP_XMPP_PROXY_ =
    'https://chromoting-httpxmpp-oauth2-dev.corp.google.com';

/**
 * The oldest API version that we support.
 * This will differ from the |API_VERSION_| if we maintain backward
 * compatibility with older API versions.
 *
 * @const
 * @private
 */
remoting.ClientSession.prototype.API_MIN_VERSION_ = 1;

/**
 * The id of the client plugin
 *
 * @const
 */
remoting.ClientSession.prototype.PLUGIN_ID = 'session-client-plugin';

/**
 * Callback to invoke when the state is changed.
 *
 * @type {function(remoting.ClientSession.State):void}
 */
remoting.ClientSession.prototype.onStateChange = function(state) { };

/**
 * Adds <embed> element to |container| and readies the sesion object.
 *
 * @param {Element} container The element to add the plugin to.
 * @param {string} oauth2AccessToken A valid OAuth2 access token.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.createPluginAndConnect =
    function(container, oauth2AccessToken) {
  this.plugin = /** @type {remoting.ViewerPlugin} */
      document.createElement('embed');
  this.plugin.id = this.PLUGIN_ID;
  this.plugin.src = 'about://none';
  this.plugin.type = 'pepper-application/x-chromoting';
  this.plugin.width = 0;
  this.plugin.height = 0;
  container.appendChild(this.plugin);

  if (!this.isPluginVersionSupported_(this.plugin)) {
    // TODO(ajwong): Remove from parent.
    delete this.plugin;
    this.setState_(remoting.ClientSession.State.BAD_PLUGIN_VERSION);
    return;
  }

  var that = this;
  this.plugin.sendIq = function(msg) { that.sendIq_(msg); };
  this.plugin.debugInfo = function(msg) {
    remoting.debug.log('plugin: ' + msg);
  };

  // TODO(ajwong): Is it even worth having this class handle these events?
  // Or would it be better to just allow users to pass in their own handlers
  // and leave these blank by default?
  this.plugin.connectionInfoUpdate = function() {
    that.connectionInfoUpdateCallback();
  };
  this.plugin.desktopSizeUpdate = function() { that.onDesktopSizeChanged_(); };

  // For IT2Me, we are pre-authorized so there is no login challenge.
  this.plugin.loginChallenge = function() {};

  // TODO(garykac): Clean exit if |connect| isn't a function.
  if (typeof this.plugin.connect === 'function') {
    this.registerConnection_(oauth2AccessToken);
  } else {
    remoting.debug.log('ERROR: remoting plugin not loaded');
    this.setState_(remoting.ClientSession.State.UNKNOWN_PLUGIN_ERROR);
  }
};

/**
 * Deletes the <embed> element from the container, without sending a
 * session_terminate request.  This is to be called when the session was
 * disconnected by the Host.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.removePlugin = function() {
  var plugin = document.getElementById(this.PLUGIN_ID);
  if (plugin) {
    var parentNode = this.plugin.parentNode;
    parentNode.removeChild(plugin);
    plugin = null;
  }
}

/**
 * Deletes the <embed> element from the container and disconnects.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.disconnect = function() {
  this.removePlugin();
  var parameters = {
    'to': this.hostJid,
    'payload_xml':
        '<jingle xmlns="urn:xmpp:jingle:1"' +
               ' action="session-terminate"' +
               ' initiator="' + this.clientJid + '"' +
               ' sid="' + this.sessionId + '">' +
          '<reason><success/></reason>' +
        '</jingle>',
    'id': 'session_terminate',
    'type': 'set',
    'host_jid': this.hostJid
  };
  this.sendIqWithParameters_(parameters);
};

/**
 * Sends an IQ stanza via the http xmpp proxy.
 *
 * @private
 * @param {string} msg XML string of IQ stanza to send to server.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendIq_ = function(msg) {
  remoting.debug.log('Sending Iq: ' + msg);

  // Extract the top level fields of the Iq packet.
  // TODO(ajwong): Can the plugin just return these fields broken out.
  var parser = new DOMParser();
  var iqNode = parser.parseFromString(msg, 'text/xml').firstChild;
  var jingleNode = iqNode.firstChild;
  var serializer = new XMLSerializer();
  var parameters = {
    'to': iqNode.getAttribute('to') || '',
    'payload_xml': serializer.serializeToString(jingleNode),
    'id': iqNode.getAttribute('id') || '1',
    'type': iqNode.getAttribute('type'),
    'host_jid': this.hostJid
  };

  this.sendIqWithParameters_(parameters);

  if (jingleNode) {
    var action = jingleNode.getAttribute('action');
    if (jingleNode.nodeName == 'jingle' && action == 'session-initiate') {
      // The session id is needed in order to close the session later.
      this.sessionId = jingleNode.getAttribute('sid');
    }
  }
};

/**
 * Sends an IQ stanza via the http xmpp proxy.
 *
 * @private
 * @param {(string|Object.<string>)} parameters Parameters to include.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendIqWithParameters_ = function(parameters) {
  remoting.xhr.post(this.HTTP_XMPP_PROXY_ + '/sendIq', function(xhr) {},
                    parameters, {}, true);
};

/**
 * Executes a poll loop on the server for more IQ packet to feed to the plugin.
 *
 * @private
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.feedIq_ = function() {
  var that = this;

  var onIq = function(xhr) {
    if (xhr.status == 200) {
      remoting.debug.log('Receiving Iq: --' + xhr.responseText + '--');
      that.plugin.onIq(xhr.responseText);
    }
    if (xhr.status == 200 || xhr.status == 204) {
      // Poll again.
      that.feedIq_();
    } else {
      remoting.debug.log('HttpXmpp gateway returned code: ' + xhr.status);
      that.plugin.disconnect();
      that.setState_(remoting.ClientSession.State.CONNECTION_FAILED);
    }
  }

  remoting.xhr.get(this.HTTP_XMPP_PROXY_ + '/readIq', onIq,
                   {'host_jid': this.hostJid}, {}, true);
};

/**
 * @private
 * @param {remoting.ViewerPlugin} plugin The embed element for the plugin.
 * @return {boolean} True if the plugin and web-app versions are compatible.
 */
remoting.ClientSession.prototype.isPluginVersionSupported_ = function(plugin) {
  return this.API_VERSION_ >= plugin.apiMinVersion &&
      plugin.apiVersion >= this.API_MIN_VERSION_;
};

/**
 * Registers a new connection with the HttpXmpp proxy.
 *
 * @private
 * @param {string} oauth2AccessToken A valid OAuth2 access token.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.registerConnection_ =
    function(oauth2AccessToken) {
  var parameters = {
    'host_jid': this.hostJid,
    'username': this.email,
    'password': oauth2AccessToken
  };

  var that = this;
  var onRegistered = function(xhr) {
    if (xhr.status != 200) {
      remoting.debug.log('FailedToConnect: --' + xhr.responseText +
                         '-- (status=' + xhr.status + ')');
      that.setState_(remoting.ClientSession.State.CONNECTION_FAILED);
      return;
    }

    remoting.debug.log('Receiving Iq: --' + xhr.responseText + '--');
    that.clientJid = xhr.responseText;

    // TODO(ajwong): Remove old version support.
    if (that.plugin.apiVersion >= 2) {
      that.plugin.connect(that.hostJid, that.hostPublicKey, that.clientJid,
                          that.accessCode);
    } else {
      that.plugin.connect(that.hostJid, that.clientJid, that.accessCode);
    }
    that.feedIq_();
  };

  remoting.xhr.post(this.HTTP_XMPP_PROXY_ + '/newConnection',
                    onRegistered, parameters, {}, true);
};

/**
 * Callback that the plugin invokes to indicate that the connection
 * status has changed.
 */
remoting.ClientSession.prototype.connectionInfoUpdateCallback = function() {
  var state = this.plugin.status;

  // TODO(ajwong): We're doing silly type translation here. Any way to avoid?
  if (state == this.plugin.STATUS_UNKNOWN) {
    this.setState_(remoting.ClientSession.State.UNKNOWN);
  } else if (state == this.plugin.STATUS_CONNECTING) {
    this.setState_(remoting.ClientSession.State.CONNECTING);
  } else if (state == this.plugin.STATUS_INITIALIZING) {
    this.setState_(remoting.ClientSession.State.INITIALIZING);
  } else if (state == this.plugin.STATUS_CONNECTED) {
    this.onDesktopSizeChanged_();
    this.setState_(remoting.ClientSession.State.CONNECTED);
  } else if (state == this.plugin.STATUS_CLOSED) {
    this.setState_(remoting.ClientSession.State.CLOSED);
  } else if (state == this.plugin.STATUS_FAILED) {
    this.setState_(remoting.ClientSession.State.CONNECTION_FAILED);
  }
};

/**
 * @private
 * @param {remoting.ClientSession.State} state The new state for the session.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.setState_ = function(state) {
  var oldState = this.state;
  this.state = state;
  if (this.onStateChange) {
    this.onStateChange(oldState);
  }
};

/**
 * This is a callback that gets called when the window is resized.
 *
 * @private
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onWindowSizeChanged = function() {
  remoting.debug.log('window size changed: ' +
                     window.innerWidth + 'x' +
                     window.innerHeight);
  this.updateDimensions();
};

/**
 * This is a callback that gets called when the plugin notifies us of a change
 * in the size of the remote desktop.
 *
 * @private
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onDesktopSizeChanged_ = function() {
  remoting.debug.log('desktop size changed: ' +
                     this.plugin.desktopWidth + 'x' +
                     this.plugin.desktopHeight);
  this.updateDimensions();
};

/**
 * Refreshes the plugin's dimensions, taking into account the sizes of the
 * remote desktop and client window, and the current scale-to-fit setting.
 *
 * @param {boolean} shouldScale If the plugin should scale itself.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.updateDimensions = function() {
  if (this.plugin.desktopWidth == 0 ||
      this.plugin.desktopHeight == 0)
    return;

  var windowWidth = window.innerWidth;
  var windowHeight = window.innerHeight;
  var scale = 1.0;

  if (remoting.scaleToFit) {
    var scaleFitHeight = 1.0 * windowHeight / this.plugin.desktopHeight;
    var scaleFitWidth = 1.0 * windowWidth / this.plugin.desktopWidth;
    scale = Math.min(1.0, scaleFitHeight, scaleFitWidth);
  }

  // Resize the plugin if necessary.
  this.plugin.width = this.plugin.desktopWidth * scale;
  this.plugin.height = this.plugin.desktopHeight * scale;

  // Position the container.
  // TODO(wez): We should take into account scrollbars when positioning.
  var parentNode = this.plugin.parentNode;
  if (this.plugin.width < windowWidth)
    parentNode.style.left = (windowWidth - this.plugin.width) / 2 + 'px';
  else
    parentNode.style.left = 0;
  if (this.plugin.height < windowHeight)
    parentNode.style.top = (windowHeight - this.plugin.height) / 2 + 'px';
  else
    parentNode.style.top = 0;

  remoting.debug.log('plugin dimensions: ' +
                     parentNode.style.left + ',' +
                     parentNode.style.top + '-' +
                     this.plugin.width + 'x' + this.plugin.height + '.');
  this.plugin.setScaleToFit(remoting.scaleToFit);
};

/**
 * Returns an associative array with a set of stats for this connection.
 *
 * @return {Object} The connection statistics.
 */
remoting.ClientSession.prototype.stats = function() {
  return {
    'video_bandwidth': this.plugin.videoBandwidth,
    'capture_latency': this.plugin.videoCaptureLatency,
    'encode_latency': this.plugin.videoEncodeLatency,
    'decode_latency': this.plugin.videoDecodeLatency,
    'render_latency': this.plugin.videoRenderLatency,
    'roundtrip_latency': this.plugin.roundTripLatency
  };
};

}());
