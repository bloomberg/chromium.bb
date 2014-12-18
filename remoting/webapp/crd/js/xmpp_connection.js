// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * A connection to an XMPP server.
 *
 * TODO(sergeyu): Chrome provides two APIs for TCP sockets: chrome.socket and
 * chrome.sockets.tcp . chrome.socket is deprecated but it's still used here
 * because TLS support in chrome.sockets.tcp is currently broken, see
 * crbug.com/403076 .
 *
 * @param {function(remoting.SignalStrategy.State):void} onStateChangedCallback
 *   Callback to call on state change.
 * @constructor
 * @implements {remoting.SignalStrategy}
 */
remoting.XmppConnection = function(onStateChangedCallback) {
  /** @private */
  this.server_ = '';
  /** @private */
  this.port_ = 0;
  /** @private */
  this.onStateChangedCallback_ = onStateChangedCallback;
  /** @type {?function(Element):void} @private */
  this.onIncomingStanzaCallback_ = null;
  /** @private */
  this.socketId_ = -1;
  /** @private */
  this.state_ = remoting.SignalStrategy.State.NOT_CONNECTED;
  /** @private */
  this.readPending_ = false;
  /** @private */
  this.sendPending_ = false;
  /** @private */
  this.startTlsPending_ = false;
  /** @type {Array.<ArrayBuffer>} @private */
  this.sendQueue_ = [];
  /** @type {remoting.XmppLoginHandler} @private*/
  this.loginHandler_ = null;
  /** @type {remoting.XmppStreamParser} @private*/
  this.streamParser_ = null;
  /** @private */
  this.jid_ = '';
  /** @private */
  this.error_ = remoting.Error.NONE;
  /** @private */
  this.fakeSslHandshake_ = false;
  /** @private */
  this.fakeSslHandshakeBytesRead_ = 0;
  /** @private */
  this.fakeSslHandshakeResponse_ =
      new Uint8Array(remoting.XmppConnection.FAKE_SSL_RESPONSE_.length);
};

/** @private */
remoting.XmppConnection.FAKE_SSL_HELLO_ = [
  0x80, 0x46,                                            // msg len
  0x01,                                                  // CLIENT_HELLO
  0x03, 0x01,                                            // SSL 3.1
  0x00, 0x2d,                                            // ciphersuite len
  0x00, 0x00,                                            // session id len
  0x00, 0x10,                                            // challenge len
  0x01, 0x00, 0x80, 0x03, 0x00, 0x80, 0x07, 0x00, 0xc0,  // ciphersuites
  0x06, 0x00, 0x40, 0x02, 0x00, 0x80, 0x04, 0x00, 0x80,  //
  0x00, 0x00, 0x04, 0x00, 0xfe, 0xff, 0x00, 0x00, 0x0a,  //
  0x00, 0xfe, 0xfe, 0x00, 0x00, 0x09, 0x00, 0x00, 0x64,  //
  0x00, 0x00, 0x62, 0x00, 0x00, 0x03, 0x00, 0x00, 0x06,  //
  0x1f, 0x17, 0x0c, 0xa6, 0x2f, 0x00, 0x78, 0xfc,        // challenge
  0x46, 0x55, 0x2e, 0xb1, 0x83, 0x39, 0xf1, 0xea         //
];

/** @private */
remoting.XmppConnection.FAKE_SSL_RESPONSE_ = [
  0x16,                                            // handshake message
  0x03, 0x01,                                      // SSL 3.1
  0x00, 0x4a,                                      // message len
  0x02,                                            // SERVER_HELLO
  0x00, 0x00, 0x46,                                // handshake len
  0x03, 0x01,                                      // SSL 3.1
  0x42, 0x85, 0x45, 0xa7, 0x27, 0xa9, 0x5d, 0xa0,  // server random
  0xb3, 0xc5, 0xe7, 0x53, 0xda, 0x48, 0x2b, 0x3f,  //
  0xc6, 0x5a, 0xca, 0x89, 0xc1, 0x58, 0x52, 0xa1,  //
  0x78, 0x3c, 0x5b, 0x17, 0x46, 0x00, 0x85, 0x3f,  //
  0x20,                                            // session id len
  0x0e, 0xd3, 0x06, 0x72, 0x5b, 0x5b, 0x1b, 0x5f,  // session id
  0x15, 0xac, 0x13, 0xf9, 0x88, 0x53, 0x9d, 0x9b,  //
  0xe8, 0x3d, 0x7b, 0x0c, 0x30, 0x32, 0x6e, 0x38,  //
  0x4d, 0xa2, 0x75, 0x57, 0x41, 0x6c, 0x34, 0x5c,  //
  0x00, 0x04,                                      // RSA/RC4-128/MD5
  0x00                                             // null compression
];

/**
 * @param {?function(Element):void} onIncomingStanzaCallback Callback to call on
 *     incoming messages.
 */
remoting.XmppConnection.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {
  this.onIncomingStanzaCallback_ = onIncomingStanzaCallback;
};

/**
 * @param {string} server
 * @param {string} username
 * @param {string} authToken
 */
remoting.XmppConnection.prototype.connect =
    function(server, username, authToken) {
  base.debug.assert(this.state_ == remoting.SignalStrategy.State.NOT_CONNECTED);

  this.error_ = remoting.Error.NONE;
  var hostnameAndPort = server.split(':', 2);
  this.server_ = hostnameAndPort[0];
  this.port_ =
      (hostnameAndPort.length == 2) ? parseInt(hostnameAndPort[1], 10) : 5222;

  // The server name is passed as to attribute in the <stream>. When connecting
  // to talk.google.com it affects the certificate the server will use for TLS:
  // talk.google.com uses gmail certificate when specified server is gmail.com
  // or googlemail.com and google.com cert otherwise. In the same time it
  // doesn't accept talk.google.com as target server. Here we use google.com
  // server name when authenticating to talk.google.com. This ensures that the
  // server will use google.com cert which will be accepted by the TLS
  // implementation in Chrome (TLS API doesn't allow specifying domain other
  // than the one that was passed to connect()).
  var xmppServer = this.server_;
  if (xmppServer == 'talk.google.com')
    xmppServer = 'google.com';

  /** @type {remoting.XmppLoginHandler} */
  this.loginHandler_ =
      new remoting.XmppLoginHandler(xmppServer, username, authToken,
                                    this.sendString_.bind(this),
                                    this.startTls_.bind(this),
                                    this.onHandshakeDone_.bind(this),
                                    this.onError_.bind(this));
  chrome.socket.create("tcp", {}, this.onSocketCreated_.bind(this));
  this.setState_(remoting.SignalStrategy.State.CONNECTING);
};

/** @param {string} message */
remoting.XmppConnection.prototype.sendMessage = function(message) {
  base.debug.assert(this.state_ == remoting.SignalStrategy.State.CONNECTED);
  this.sendString_(message);
};

/** @return {remoting.SignalStrategy.State} Current state */
remoting.XmppConnection.prototype.getState = function() {
  return this.state_;
};

/** @return {remoting.Error} Error when in FAILED state. */
remoting.XmppConnection.prototype.getError = function() {
  return this.error_;
};

/** @return {string} Current JID when in CONNECTED state. */
remoting.XmppConnection.prototype.getJid = function() {
  return this.jid_;
};

remoting.XmppConnection.prototype.dispose = function() {
  this.closeSocket_();
  this.setState_(remoting.SignalStrategy.State.CLOSED);
};

/**
 * @param {chrome.socket.CreateInfo} createInfo
 * @private
 */
remoting.XmppConnection.prototype.onSocketCreated_ = function(createInfo) {
  // Check if connection was destroyed.
  if (this.state_ != remoting.SignalStrategy.State.CONNECTING) {
    chrome.socket.destroy(createInfo.socketId);
    return;
  }

  this.socketId_ = createInfo.socketId;

  chrome.socket.connect(this.socketId_,
                        this.server_,
                        this.port_,
                        this.onSocketConnected_.bind(this));
};

/**
 * @param {number} result
 * @private
 */
remoting.XmppConnection.prototype.onSocketConnected_ = function(result) {
  // Check if connection was destroyed.
  if (this.state_ != remoting.SignalStrategy.State.CONNECTING) {
    return;
  }

  if (result != 0) {
    this.onError_(remoting.Error.NETWORK_FAILURE,
                  'Failed to connect to ' + this.server_ + ': ' +  result);
    return;
  }

  this.setState_(remoting.SignalStrategy.State.HANDSHAKE);

  // Initiate fake SSL handshake if we are on port 443. It's used to bypass
  // firewalls that block port 5222, but allow HTTPS traffic.
  this.fakeSslHandshake_ = (this.port_ == 443);
  if (this.fakeSslHandshake_) {
    this.sendBuffer_(
        new Uint8Array(remoting.XmppConnection.FAKE_SSL_HELLO_).buffer);
  } else {
    this.loginHandler_.start();
  }

  this.tryRead_();
};

/**
 * @private
 */
remoting.XmppConnection.prototype.tryRead_ = function() {
  base.debug.assert(!this.readPending_);
  base.debug.assert(this.state_ == remoting.SignalStrategy.State.HANDSHAKE ||
                    this.state_ == remoting.SignalStrategy.State.CONNECTED);
  base.debug.assert(!this.startTlsPending_);

  this.readPending_ = true;
  chrome.socket.read(this.socketId_, this.onRead_.bind(this));
};

/**
 * @param {chrome.socket.ReadInfo} readInfo
 * @private
 */
remoting.XmppConnection.prototype.onRead_ = function(readInfo) {
  base.debug.assert(this.readPending_);
  this.readPending_ = false;

  // Check if the socket was closed while reading.
  if (this.state_ != remoting.SignalStrategy.State.HANDSHAKE &&
      this.state_ != remoting.SignalStrategy.State.CONNECTED) {
    return;
  }

  if (readInfo.resultCode < 0) {
    this.onError_(remoting.Error.NETWORK_FAILURE,
                  'Failed to receive from XMPP socket: ' + readInfo.resultCode);
    return;
  }

  if (this.state_ == remoting.SignalStrategy.State.HANDSHAKE) {
    var data = readInfo.data;

    if (this.fakeSslHandshake_) {
      var bytesExpected = remoting.XmppConnection.FAKE_SSL_RESPONSE_.length -
                          this.fakeSslHandshakeBytesRead_;
      var bytesConsumed = Math.min(bytesExpected, readInfo.data.byteLength);
      this.fakeSslHandshakeResponse_.set(
          new Uint8Array(data.slice(0, bytesConsumed)),
          this.fakeSslHandshakeBytesRead_);
      this.fakeSslHandshakeBytesRead_ += bytesConsumed;
      data = data.slice(bytesConsumed);

      // Check if we are finished with the fake SSL handshake.
      if (this.fakeSslHandshakeBytesRead_ ==
          remoting.XmppConnection.FAKE_SSL_RESPONSE_.length) {
        this.fakeSslHandshake_ = false;
        this.loginHandler_.start();

        // Compare the response to what's expected and log an error if they
        // don't match.
        var responseMatches = true;
        for (var i = 0; i < this.fakeSslHandshakeResponse_.length; ++i) {
          if (this.fakeSslHandshakeResponse_[i] !=
              remoting.XmppConnection.FAKE_SSL_RESPONSE_[i]) {
            responseMatches = false;
            break;
          }
        }

        if (!responseMatches) {
          var formatted = "";
          for (var i = 0; i < this.fakeSslHandshakeResponse_.length; ++i) {
            var x = /** @type {number} */ this.fakeSslHandshakeResponse_[i];
            formatted += " 0x" + x.toString(16);
          };
          console.error(
              "XmppConnection: Fake SSL response didn't match what was " +
              "expected. Trying to continue anyway. Received response: [" +
              formatted + "]");
        }
      }
    }

    // Once fake SSL handshake is finished pass the data to the login handler.
    if (!this.fakeSslHandshake_ && data.byteLength > 0) {
      this.loginHandler_.onDataReceived(data);
    }
  } else if (this.state_ == remoting.SignalStrategy.State.CONNECTED) {
    this.streamParser_.appendData(readInfo.data);
  }

  if (!this.startTlsPending_) {
    this.tryRead_();
  }
};

/**
 * @param {string} text
 * @private
 */
remoting.XmppConnection.prototype.sendString_ = function(text) {
  this.sendBuffer_(base.encodeUtf8(text));
};

/**
 * @param {ArrayBuffer} data
 * @private
 */
remoting.XmppConnection.prototype.sendBuffer_ = function(data) {
  this.sendQueue_.push(data);
  this.flushSendQueue_();
};

/**
 * @private
 */
remoting.XmppConnection.prototype.flushSendQueue_ = function() {
  if (this.sendPending_ || this.sendQueue_.length == 0) {
    return;
  }

  var data = this.sendQueue_[0]
  this.sendPending_ = true;
  chrome.socket.write(this.socketId_, data, this.onWrite_.bind(this));
};

/**
 * @param {chrome.socket.WriteInfo} writeInfo
 * @private
 */
remoting.XmppConnection.prototype.onWrite_ = function(writeInfo) {
  base.debug.assert(this.sendPending_);
  this.sendPending_ = false;

  // Ignore write() result if the socket was closed.
  if (this.state_ != remoting.SignalStrategy.State.HANDSHAKE &&
      this.state_ != remoting.SignalStrategy.State.CONNECTED) {
    return;
  }

  if (writeInfo.bytesWritten < 0) {
    this.onError_(remoting.Error.NETWORK_FAILURE,
                  'TCP write failed with error ' + writeInfo.bytesWritten);
    return;
  }

  base.debug.assert(this.sendQueue_.length > 0);

  var data = this.sendQueue_[0]
  base.debug.assert(writeInfo.bytesWritten <= data.byteLength);
  if (writeInfo.bytesWritten == data.byteLength) {
    this.sendQueue_.shift();
  } else {
    this.sendQueue_[0] = data.slice(data.byteLength - writeInfo.bytesWritten);
  }

  this.flushSendQueue_();
};

/**
 * @private
 */
remoting.XmppConnection.prototype.startTls_ = function() {
  base.debug.assert(!this.readPending_);
  base.debug.assert(!this.startTlsPending_);

  this.startTlsPending_ = true;
  chrome.socket.secure(
      this.socketId_, {}, this.onTlsStarted_.bind(this));
}

/**
 * @param {number} resultCode
 * @private
 */
remoting.XmppConnection.prototype.onTlsStarted_ = function(resultCode) {
  base.debug.assert(this.startTlsPending_);
  this.startTlsPending_ = false;

  if (resultCode < 0) {
    this.onError_(remoting.Error.NETWORK_FAILURE,
                  'Failed to start TLS: ' + resultCode);
    return;
  }

  this.tryRead_();
  this.loginHandler_.onTlsStarted();
};

/**
 * @param {string} jid
 * @param {remoting.XmppStreamParser} streamParser
 * @private
 */
remoting.XmppConnection.prototype.onHandshakeDone_ =
    function(jid, streamParser) {
  this.jid_ = jid;
  this.streamParser_ = streamParser;
  this.streamParser_.setCallbacks(this.onIncomingStanza_.bind(this),
                                  this.onParserError_.bind(this));
  this.setState_(remoting.SignalStrategy.State.CONNECTED);
};

/**
 * @param {Element} stanza
 * @private
 */
remoting.XmppConnection.prototype.onIncomingStanza_ = function(stanza) {
  if (this.onIncomingStanzaCallback_) {
    this.onIncomingStanzaCallback_(stanza);
  }
};

/**
 * @param {string} text
 * @private
 */
remoting.XmppConnection.prototype.onParserError_ = function(text) {
  this.onError_(remoting.Error.UNEXPECTED, text);
}

/**
 * @param {remoting.Error} error
 * @param {string} text
 * @private
 */
remoting.XmppConnection.prototype.onError_ = function(error, text) {
  console.error(text);
  this.error_ = error;
  this.closeSocket_();
  this.setState_(remoting.SignalStrategy.State.FAILED);
};

/**
 * @private
 */
remoting.XmppConnection.prototype.closeSocket_ = function() {
  if (this.socketId_ != -1) {
    chrome.socket.destroy(this.socketId_);
    this.socketId_ = -1;
  }
};

/**
 * @param {remoting.SignalStrategy.State} newState
 * @private
 */
remoting.XmppConnection.prototype.setState_ = function(newState) {
  if (this.state_ != newState) {
    this.state_ = newState;
    this.onStateChangedCallback_(this.state_);
  }
};
