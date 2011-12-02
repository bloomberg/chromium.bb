// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A class of server log entries.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @private
 * @constructor
 */
remoting.ServerLogEntry = function() {
  /** @type Object.<string, string> */ this.dict = {};
};

/** @private */
remoting.ServerLogEntry.prototype.KEY_EVENT_NAME_ = 'event-name';
/** @private */
remoting.ServerLogEntry.prototype.VALUE_EVENT_NAME_SESSION_STATE_ =
    'session-state';

/** @private */
remoting.ServerLogEntry.prototype.KEY_ID_ = 'id';

/** @private */
remoting.ServerLogEntry.prototype.KEY_ROLE_ = 'role';
/** @private */
remoting.ServerLogEntry.prototype.VALUE_ROLE_CLIENT_ = 'client';

/** @private */
remoting.ServerLogEntry.prototype.KEY_SESSION_STATE_ = 'session-state';

/**
 * @private
 * @param {remoting.ClientSession.State} state
 * @return {string}
 */
remoting.ServerLogEntry.prototype.getValueForSessionState = function(state) {
  switch(state) {
    case remoting.ClientSession.State.UNKNOWN:
      return 'unknown';
    case remoting.ClientSession.State.CREATED:
      return 'created';
    case remoting.ClientSession.State.BAD_PLUGIN_VERSION:
      return 'bad-plugin-version';
    case remoting.ClientSession.State.UNKNOWN_PLUGIN_ERROR:
      return 'unknown-plugin-error';
    case remoting.ClientSession.State.CONNECTING:
      return 'connecting';
    case remoting.ClientSession.State.INITIALIZING:
      return 'initializing';
    case remoting.ClientSession.State.CONNECTED:
      return 'connected';
    case remoting.ClientSession.State.CLOSED:
      return 'closed';
    case remoting.ClientSession.State.CONNECTION_FAILED:
      return 'connection-failed';
    default:
      return 'undefined-' + state;
  }
};

/** @private */
remoting.ServerLogEntry.prototype.KEY_CONNECTION_ERROR_ = 'connection-error';

/**
 * @private
 * @param {remoting.ClientSession.ConnectionError} connectionError
 * @return {string}
 */
remoting.ServerLogEntry.prototype.getValueForConnectionError =
    function(connectionError) {
  switch(connectionError) {
    case remoting.ClientSession.ConnectionError.NONE:
      return 'none';
    case remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE:
      return 'host-is-offline';
    case remoting.ClientSession.ConnectionError.SESSION_REJECTED:
      return 'session-rejected';
    case remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL:
      return 'incompatible-protocol';
    case remoting.ClientSession.ConnectionError.NETWORK_FAILURE:
      return 'network-failure';
    default:
      return 'unknown-' + connectionError;
  }
}

/** @private */
remoting.ServerLogEntry.prototype.KEY_OS_NAME_ = 'os-name';
/** @private */
remoting.ServerLogEntry.prototype.VALUE_OS_NAME_WINDOWS_ = 'Windows';
/** @private */
remoting.ServerLogEntry.prototype.VALUE_OS_NAME_LINUX_ = 'Linux';
/** @private */
remoting.ServerLogEntry.prototype.VALUE_OS_NAME_MAC_ = 'Mac';
/** @private */
remoting.ServerLogEntry.prototype.VALUE_OS_NAME_CHROMEOS_ = 'ChromeOS';

/** @private */
remoting.ServerLogEntry.prototype.KEY_OS_VERSION_ = 'os-version';

/** @private */
remoting.ServerLogEntry.prototype.KEY_CPU_ = 'cpu';

/** @private */
remoting.ServerLogEntry.prototype.KEY_BROWSER_VERSION_ = 'browser-version';

/** @private */
remoting.ServerLogEntry.prototype.KEY_WEBAPP_VERSION_ = 'webapp-version';

/**
 * Sets one field in this log entry.
 *
 * @private
 * @param {string} key
 * @param {string} value
 */
remoting.ServerLogEntry.prototype.set = function(key, value) {
  this.dict[key] = value;
};

/**
 * Converts this object into an XML stanza.
 *
 * @return {string}
 */
remoting.ServerLogEntry.prototype.toStanza = function() {
  var stanza = '<gr:entry ';
  for (var key in this.dict) {
    stanza += escape(key) + '="' + escape(this.dict[key]) + '" ';
  }
  stanza += '/>';
  return stanza;
};

/**
 * Makes a log entry for a change of client session state.
 *
 * @param {remoting.ClientSession.State} state
 * @param {remoting.ClientSession.ConnectionError} connectionError
 * @return {remoting.ServerLogEntry}
 */
remoting.ServerLogEntry.prototype.makeClientSessionStateChange =
    function(state, connectionError) {
  var entry = new remoting.ServerLogEntry();
  entry.set(this.KEY_ROLE_, this.VALUE_ROLE_CLIENT_);
  entry.set(this.KEY_EVENT_NAME_, this.VALUE_EVENT_NAME_SESSION_STATE_);
  entry.set(this.KEY_SESSION_STATE_, this.getValueForSessionState(state));
  if (connectionError != remoting.ClientSession.ConnectionError.NONE) {
    entry.set(this.KEY_CONNECTION_ERROR_,
              this.getValueForConnectionError(connectionError));
  }
  return entry;
};

/**
 * Adds an ID field to this log entry.
 *
 * @param {string} id
 */
remoting.ServerLogEntry.prototype.addIdField = function(id) {
  this.set(this.KEY_ID_, id);
}

/**
 * Adds fields describing the host to this log entry.
 */
remoting.ServerLogEntry.prototype.addHostFields = function() {
  var host = this.getHostData();
  if (host) {
    if (host.os_name.length > 0) {
      this.set(this.KEY_OS_NAME_, host.os_name);
    }
    if (host.os_version.length > 0) {
      this.set(this.KEY_OS_VERSION_, host.os_version);
    }
    if (host.cpu.length > 0) {
      this.set(this.KEY_CPU_, host.cpu);
    }
  }
};

/**
 * Extracts host data from the userAgent string.
 *
 * @private
 * @return {{os_name:string, os_version:string, cpu:string} | null}
 */
remoting.ServerLogEntry.prototype.getHostData = function() {
  return this.extractHostDataFrom(navigator.userAgent);
};

/**
 * Extracts host data from the given userAgent string.
 *
 * @private
 * @param {string} s
 * @return {{os_name:string, os_version:string, cpu:string} | null}
 */
remoting.ServerLogEntry.prototype.extractHostDataFrom = function(s) {
  // Sample userAgent strings:
  // 'Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/535.2 ' +
  //   '(KHTML, like Gecko) Chrome/15.0.874.106 Safari/535.2'
  // 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/535.8 ' +
  //   '(KHTML, like Gecko) Chrome/17.0.933.0 Safari/535.8'
  // 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_8) AppleWebKit/535.1 ' +
  //   '(KHTML, like Gecko) Chrome/14.0.835.202 Safari/535.1'
  // 'Mozilla/5.0 (X11; CrOS i686 14.811.154) AppleWebKit/535.1 ' +
  //   '(KHTML, like Gecko) Chrome/14.0.835.204 Safari/535.1'
  var match = new RegExp('Windows NT ([0-9\\.]*)').exec(s);
  if (match && (match.length >= 2)) {
    return {
        'os_name': this.VALUE_OS_NAME_WINDOWS_,
        'os_version': match[1],
        'cpu': ''
    };
  }
  match = new RegExp('Linux ([a-zA-Z0-9_]*)').exec(s);
  if (match && (match.length >= 2)) {
    return {
        'os_name': this.VALUE_OS_NAME_LINUX_,
        'os_version' : '',
        'cpu': match[1]
    };
  }
  match = new RegExp('([a-zA-Z]*) Mac OS X ([0-9_]*)').exec(s);
  if (match && (match.length >= 3)) {
    return {
        'os_name': this.VALUE_OS_NAME_MAC_,
        'os_version': match[2].replace(/_/g, '.'),
        'cpu': match[1]
    };
  }
  match = new RegExp('CrOS ([a-zA-Z0-9]*) ([0-9.]*)').exec(s);
  if (match && (match.length >= 3)) {
    return {
        'os_name': this.VALUE_OS_NAME_CHROMEOS_,
        'os_version': match[2],
        'cpu': match[1]
    };
  }
  return null;
};

/**
 * Adds a field specifying the browser version to this log entry.
 */
remoting.ServerLogEntry.prototype.addChromeVersionField = function() {
  var version = this.getChromeVersion();
  if (version != null) {
    this.set(this.KEY_BROWSER_VERSION_, version);
  }
};

/**
 * Extracts the Chrome version from the userAgent string.
 *
 * @private
 * @return {string | null}
 */
remoting.ServerLogEntry.prototype.getChromeVersion = function() {
  return this.extractChromeVersionFrom(navigator.userAgent);
};

/**
 * Extracts the Chrome version from the given userAgent string.
 *
 * @private
 * @param {string} s
 * @return {string | null}
 */
remoting.ServerLogEntry.prototype.extractChromeVersionFrom = function(s) {
  var match = new RegExp('Chrome/([0-9.]*)').exec(s);
  if (match && (match.length >= 2)) {
    return match[1];
  }
  return null;
};

/**
 * Adds a field specifying the webapp version to this log entry.
 */
remoting.ServerLogEntry.prototype.addWebappVersionField = function() {
  this.set(this.KEY_WEBAPP_VERSION_, chrome.app.getDetails().version);
};
