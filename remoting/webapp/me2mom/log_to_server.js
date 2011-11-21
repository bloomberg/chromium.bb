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
  /** @type boolean */ this.enabled = false;
};

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
  this.log(entry);
};

/**
 * Sends a log entry to the server.
 *
 * @private
 * @param {remoting.ServerLogEntry} entry
 */
remoting.LogToServer.prototype.log = function(entry) {
  if (!this.enabled) {
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
