// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Module for sending log entries to the server.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.LogToServer = function() {
  /** @type Array.<string> */ this.pendingEntries = [];
};

// Local storage keys.
/** @private */
remoting.LogToServer.prototype.KEY_ENABLED_ = 'remoting.LogToServer.enabled';
/** @private */
remoting.LogToServer.prototype.KEY_ID_ = 'remoting.LogToServer.id';

// Constants used for generating an ID.
/** @private */
remoting.LogToServer.prototype.ID_ALPHABET_ =
    'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890';
/** @private */
remoting.LogToServer.prototype.ID_LEN_ = 20;

/**
 * Enables or disables logging.
 *
 * @param {boolean} enabled whether logging is enabled
 */
remoting.LogToServer.prototype.setEnabled = function(enabled) {
  window.localStorage.setItem(this.KEY_ENABLED_, enabled ? 'true' : 'false');
}

/**
 * Logs a client session state change.
 *
 * @param {remoting.ClientSession.State} state
 * @param {remoting.ClientSession.ConnectionError} connectionError
 */
remoting.LogToServer.prototype.logClientSessionStateChange =
    function(state, connectionError) {
  var entry = remoting.ServerLogEntry.prototype.makeClientSessionStateChange(
      state, connectionError);
  entry.addHostFields();
  entry.addChromeVersionField();
  entry.addWebappVersionField();
  entry.addIdField(this.getId());
  this.log(entry);
};

/**
 * Sends a log entry to the server.
 *
 * @private
 * @param {remoting.ServerLogEntry} entry
 */
remoting.LogToServer.prototype.log = function(entry) {
  if (!this.isEnabled()) {
    return;
  }
  // Store a stanza for the entry
  this.pendingEntries.push(entry.toStanza());
  // Stop if there's no connection to the server.
  if (!remoting.wcs) {
    return;
  }
  // Send all pending entries to the server.
  var stanza = '<cli:iq to="remoting@bot.talk.google.com" type="set" ' +
      'xmlns:cli="jabber:client"><gr:log xmlns:gr="google:remoting">';
  while (this.pendingEntries.length > 0) {
    stanza += /** @type string */ this.pendingEntries.shift();
  }
  stanza += '</gr:log></cli:iq>';
  remoting.debug.logIq(true, stanza);
  remoting.wcs.sendIq(stanza);
};

/**
 * Whether logging is enabled.
 *
 * @private
 * @return {boolean} whether logging is enabled
 */
remoting.LogToServer.prototype.isEnabled = function() {
  var value = window.localStorage.getItem(this.KEY_ENABLED_);
  return (value == 'true');
};

/**
 * Gets an ID from local storage.
 *
 * This function returns the empty string if logging is disabled.
 * If logging is enabled, and there is no ID in local storage, then this
 * function will create and store an ID.
 *
 * @private
 * @return {string} an ID, or the empty string
 */
remoting.LogToServer.prototype.getId = function() {
  if (!this.isEnabled()) {
    return '';
  }
  var id = window.localStorage.getItem(this.KEY_ID_);
  if ((!id) || (typeof id != 'string')) {
    id = this.generateId();
    window.localStorage.setItem(this.KEY_ID_, id);
  }
  return id.toString();
};

/**
 * Generates an ID.
 *
 * @private
 * @return {string} an ID
 */
remoting.LogToServer.prototype.generateId = function() {
  var idArray = [];
  for (var i = 0; i < this.ID_LEN_; i++) {
    var index = Math.random() * this.ID_ALPHABET_.length;
    idArray.push(this.ID_ALPHABET_.slice(index, index + 1));
  }
  return idArray.join('');
};
