// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * XmppLoginHandler handles authentication handshake for XmppConnection. It
 * receives incoming data using onDataReceived(), calls |sendMessageCallback|
 * to send outgoing messages and calls |onHandshakeDoneCallback| after
 * authentication is finished successfully or |onErrorCallback| on error.
 *
 * See RFC3920 for description of XMPP and authentication handshake.
 *
 * @param {string} server Domain name of the server we are connecting to.
 * @param {string} username Username.
 * @param {string} authToken OAuth2 token.
 * @param {function(string):void} sendMessageCallback Callback to call to send
 *     a message.
 * @param {function():void} startTlsCallback Callback to call to start TLS on
 *     the underlying socket.
 * @param {function(string, remoting.XmppStreamParser):void}
 *     onHandshakeDoneCallback Callback to call after authentication is
 *     completed successfully
 * @param {function(remoting.Error, string):void} onErrorCallback Callback to
 *     call on error. Can be called at any point during lifetime of connection.
 * @constructor
 */
remoting.XmppLoginHandler = function(server,
                                     username,
                                     authToken,
                                     sendMessageCallback,
                                     startTlsCallback,
                                     onHandshakeDoneCallback,
                                     onErrorCallback) {
  /** @private */
  this.server_ = server;
  /** @private */
  this.username_ = username;
  /** @private */
  this.authToken_ = authToken;
  /** @private */
  this.sendMessageCallback_ = sendMessageCallback;
  /** @private */
  this.startTlsCallback_ = startTlsCallback;
  /** @private */
  this.onHandshakeDoneCallback_ = onHandshakeDoneCallback;
  /** @private */
  this.onErrorCallback_ = onErrorCallback;

  /** @private */
  this.state_ = remoting.XmppLoginHandler.State.INIT;
  /** @private */
  this.jid_ = '';

  /** @type {remoting.XmppStreamParser} @private */
  this.streamParser_ = null;
}

/**
 * States the handshake goes through. States are iterated from INIT to DONE
 * sequentially, except for ERROR state which may be accepted at any point.
 *
 * Following messages are sent/received in each state:
 *    INIT
 *       client -> server: Stream header
 *    START_SENT
 *       client <- server: Stream header with list of supported features which
 *           should include starttls.
 *       client -> server: <starttls>
 *    STARTTLS_SENT
 *        client <- server: <proceed>
 *    STARTING_TLS
 *      TLS handshake
 *      client -> server: Stream header
 *    START_SENT_AFTER_TLS
 *      client <- server: Stream header with list of supported authentication
 *          methods which is expected to include X-OAUTH2
 *      client -> server: <auth> message with the OAuth2 token.
 *    AUTH_SENT
 *      client <- server: <success> or <failure>
 *      client -> server: Stream header
 *    AUTH_ACCEPTED
 *      client <- server: Stream header with list of features that should
 *         include <bind>.
 *      client -> server: <bind>
 *    BIND_SENT
 *      client <- server: <bind> result with JID.
 *      client -> server: <iq><session/></iq> to start the session
 *    SESSION_IQ_SENT
 *      client <- server: iq result
 *    DONE
 *
 * @enum {number}
 */
remoting.XmppLoginHandler.State = {
  INIT: 0,
  START_SENT: 1,
  STARTTLS_SENT: 2,
  STARTING_TLS: 3,
  START_SENT_AFTER_TLS: 4,
  AUTH_SENT: 5,
  AUTH_ACCEPTED: 6,
  BIND_SENT: 7,
  SESSION_IQ_SENT: 8,
  DONE: 9,
  ERROR: 10
};

remoting.XmppLoginHandler.prototype.start = function() {
  this.state_ = remoting.XmppLoginHandler.State.START_SENT;
  this.startStream_();
}

/** @param {ArrayBuffer} data */
remoting.XmppLoginHandler.prototype.onDataReceived = function(data) {
  base.debug.assert(this.state_ != remoting.XmppLoginHandler.State.INIT &&
                    this.state_ != remoting.XmppLoginHandler.State.DONE &&
                    this.state_ != remoting.XmppLoginHandler.State.ERROR);

  this.streamParser_.appendData(data);
}

/**
 * @param {Element} stanza
 * @private
 */
remoting.XmppLoginHandler.prototype.onStanza_ = function(stanza) {
  switch (this.state_) {
    case remoting.XmppLoginHandler.State.START_SENT:
      if (stanza.querySelector('features>starttls')) {
        this.sendMessageCallback_(
            '<starttls xmlns="urn:ietf:params:xml:ns:xmpp-tls"/>');
        this.state_ = remoting.XmppLoginHandler.State.STARTTLS_SENT;
      } else {
        this.onError_(remoting.Error.UNEXPECTED, "Server doesn't support TLS.");
      }
      break;

    case remoting.XmppLoginHandler.State.STARTTLS_SENT:
      if (stanza.localName == "proceed") {
        this.state_ = remoting.XmppLoginHandler.State.STARTING_TLS;
        this.startTlsCallback_();
      } else {
        this.onError_(remoting.Error.UNEXPECTED,
                      "Failed to start TLS: " +
                          (new XMLSerializer().serializeToString(stanza)));
      }
      break;

    case remoting.XmppLoginHandler.State.START_SENT_AFTER_TLS:
      var mechanisms = Array.prototype.map.call(
          stanza.querySelectorAll('features>mechanisms>mechanism'),
          /** @param {Element} m */
          function(m) { return m.textContent; });
      if (mechanisms.indexOf("X-OAUTH2")) {
        this.onError_(remoting.Error.UNEXPECTED,
                      "OAuth2 is not supported by the server.");
        return;
      }

      var cookie = window.btoa("\0" + this.username_ + "\0" + this.authToken_);

      this.state_ = remoting.XmppLoginHandler.State.AUTH_SENT;
      this.sendMessageCallback_(
          '<auth xmlns="urn:ietf:params:xml:ns:xmpp-sasl" ' +
                 'mechanism="X-OAUTH2" auth:service="oauth2" ' +
                 'auth:allow-generated-jid="true" ' +
                 'auth:client-uses-full-bind-result="true" ' +
                 'auth:allow-non-google-login="true" ' +
                 'xmlns:auth="http://www.google.com/talk/protocol/auth">' +
            cookie +
          '</auth>');
      break;

    case remoting.XmppLoginHandler.State.AUTH_SENT:
      if (stanza.localName == 'success') {
        this.state_ = remoting.XmppLoginHandler.State.AUTH_ACCEPTED;
        this.startStream_();
      } else {
        this.onError_(remoting.Error.AUTHENTICATION_FAILED,
                      'Failed to authenticate: ' +
                          (new XMLSerializer().serializeToString(stanza)));
      }
      break;

    case remoting.XmppLoginHandler.State.AUTH_ACCEPTED:
      if (stanza.querySelector('features>bind')) {
        this.sendMessageCallback_(
            '<iq type="set" id="0">' +
              '<bind xmlns="urn:ietf:params:xml:ns:xmpp-bind">' +
                '<resource>chromoting</resource>'+
              '</bind>' +
            '</iq>');
        this.state_ = remoting.XmppLoginHandler.State.BIND_SENT;
      } else {
        this.onError_(remoting.Error.UNEXPECTED,
                      "Server doesn't support bind after authentication.");
      }
      break;

    case remoting.XmppLoginHandler.State.BIND_SENT:
      var jidElement = stanza.querySelector('iq>bind>jid');
      if (stanza.getAttribute('id') != '0' ||
          stanza.getAttribute('type') != 'result' || !jidElement) {
        this.onError_(remoting.Error.UNEXPECTED,
                      'Received unexpected response to bind: ' +
                          (new XMLSerializer().serializeToString(stanza)));
        return;
      }
      this.jid_ = jidElement.textContent;
      this.sendMessageCallback_(
          '<iq type="set" id="1">' +
            '<session xmlns="urn:ietf:params:xml:ns:xmpp-session"/>' +
          '</iq>');
      this.state_ = remoting.XmppLoginHandler.State.SESSION_IQ_SENT;
      break;

    case remoting.XmppLoginHandler.State.SESSION_IQ_SENT:
      if (stanza.getAttribute('id') != '1' ||
          stanza.getAttribute('type') != 'result') {
        this.onError_(remoting.Error.UNEXPECTED,
                      'Failed to start session: ' +
                          (new XMLSerializer().serializeToString(stanza)));
        return;
      }
      this.state_ = remoting.XmppLoginHandler.State.DONE;
      this.onHandshakeDoneCallback_(this.jid_, this.streamParser_);
      break;

    default:
      base.debug.assert(false);
      break;
  }
}

remoting.XmppLoginHandler.prototype.onTlsStarted = function() {
  base.debug.assert(this.state_ ==
                    remoting.XmppLoginHandler.State.STARTING_TLS);
  this.state_ = remoting.XmppLoginHandler.State.START_SENT_AFTER_TLS;
  this.startStream_();
};

/**
 * @param {string} text
 * @private
 */
remoting.XmppLoginHandler.prototype.onParserError_ = function(text) {
  this.onError_(remoting.Error.UNEXPECTED, text);
}

/**
 * @private
 */
remoting.XmppLoginHandler.prototype.startStream_ = function() {
  this.sendMessageCallback_('<stream:stream to="' + this.server_ +
                            '" version="1.0" xmlns="jabber:client" ' +
                            'xmlns:stream="http://etherx.jabber.org/streams">');
  this.streamParser_ = new remoting.XmppStreamParser();
  this.streamParser_.setCallbacks(this.onStanza_.bind(this),
                                  this.onParserError_.bind(this));
}

/**
 * @param {remoting.Error} error
 * @param {string} text
 * @private
 */
remoting.XmppLoginHandler.prototype.onError_ = function(error, text) {
  if (this.state_ != remoting.XmppLoginHandler.State.ERROR) {
    this.onErrorCallback_(error, text);
    this.state_ = remoting.XmppLoginHandler.State.ERROR;
  } else {
    console.error(text);
  }
}
