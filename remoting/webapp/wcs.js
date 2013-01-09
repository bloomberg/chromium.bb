/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @fileoverview
 * A class that provides an interface to a WCS connection.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @type {remoting.Wcs} */
remoting.wcs = null;

/**
 * @constructor
 * @param {remoting.WcsIqClient} wcsIqClient The WCS client.
 * @param {string} token An OAuth2 access token.
 * @param {function(boolean): void} onReady A function called when the WCS
 *     client has received a full JID.
 */
remoting.Wcs = function(wcsIqClient, token, onReady) {
  /**
   * The WCS client.
   * @type {remoting.WcsIqClient}
   * @private
   */
  this.wcsIqClient_ = wcsIqClient;

  /**
   * The OAuth2 access token.
   * @type {string}
   * @private
   */
  this.token_ = token;

  /**
   * The function called when the WCS client has received a full JID.
   * @type {function(boolean): void}
   * @private
   */
  this.onReady_ = onReady;

  /**
   * The full JID of the WCS client.
   * @type {string}
   * @private
   */
  this.clientFullJid_ = '';

  var updateAccessToken = this.updateAccessToken_.bind(this);
  /** @param {remoting.Error} error */
  var onError = function(error) {
    console.error('updateAccessToken: Authentication failed: ' + error);
  };

  /**
   * A timer that polls for an updated access token.
   * @type {number}
   * @private
   */
  this.pollForUpdatedToken_ = setInterval(
      function() {
        remoting.identity.callWithToken(updateAccessToken, onError);
      },
      60 * 1000);

  /**
   * A function called when an IQ stanza is received.
   * @param {string} stanza The IQ stanza.
   * @private
   */
  this.onIq_ = function(stanza) {};

  // Handle messages from the WcsIqClient.
  this.wcsIqClient_.setOnMessage(this.onMessage_.bind(this));

  // Start the WcsIqClient.
  this.wcsIqClient_.connectChannel();
};

/**
 * Passes an access token to the WcsIqClient, if the token has been updated.
 *
 * @param {string} tokenNew A (possibly updated) access token.
 * @return {void} Nothing.
 * @private
 */
remoting.Wcs.prototype.updateAccessToken_ = function(tokenNew) {
  if (tokenNew != this.token_) {
    this.token_ = tokenNew;
    this.wcsIqClient_.updateAccessToken(this.token_);
  }
};

/**
 * Handles a message coming from the WcsIqClient.
 *
 * @param {Array.<string>} msg The message.
 * @return {void} Nothing.
 * @private
 */
remoting.Wcs.prototype.onMessage_ = function(msg) {
  if (msg[0] == 'is') {
    this.onIq_(msg[1]);
  } else if (msg[0] == 'cfj') {
    this.clientFullJid_ = msg[1];
    console.log('Received JID: ' + this.clientFullJid_);
    this.onReady_(true);
    this.onReady_ = function(success) {};
  }
};

/**
 * Gets the full JID of the WCS client.
 *
 * @return {string} The full JID.
 */
remoting.Wcs.prototype.getJid = function() {
  return this.clientFullJid_;
};

/**
 * Sends an IQ stanza.
 *
 * @param {string} stanza An IQ stanza.
 * @return {void} Nothing.
 */
remoting.Wcs.prototype.sendIq = function(stanza) {
  this.wcsIqClient_.sendIq(stanza);
};

/**
 * Sets the function called when an IQ stanza is received.
 *
 * @param {function(string): void} onIq The function called when an IQ stanza
 *     is received.
 * @return {void} Nothing.
 */
remoting.Wcs.prototype.setOnIq = function(onIq) {
  this.onIq_ = onIq;
};
