// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
remoting.ServerLogEntry.KEY_EVENT_NAME_ = 'event-name';
/** @private */
remoting.ServerLogEntry.VALUE_EVENT_NAME_SESSION_STATE_ =
    'session-state';

/** @private */
remoting.ServerLogEntry.KEY_SESSION_ID_ = 'session-id';

/** @private */
remoting.ServerLogEntry.KEY_ROLE_ = 'role';
/** @private */
remoting.ServerLogEntry.VALUE_ROLE_CLIENT_ = 'client';

/** @private */
remoting.ServerLogEntry.KEY_SESSION_STATE_ = 'session-state';

/**
 * @private
 * @param {remoting.ClientSession.State} state
 * @return {string}
 */
remoting.ServerLogEntry.getValueForSessionState = function(state) {
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
    case remoting.ClientSession.State.FAILED:
      return 'connection-failed';
    case remoting.ClientSession.State.CONNECTION_DROPPED:
      return 'connection-dropped';
    case remoting.ClientSession.State.CONNECTION_CANCELED:
      return 'connection-canceled';
    default:
      return 'undefined-' + state;
  }
};

/** @private */
remoting.ServerLogEntry.KEY_CONNECTION_ERROR_ = 'connection-error';

/**
 * @private
 * @param {remoting.ClientSession.ConnectionError} connectionError
 * @return {string}
 */
remoting.ServerLogEntry.getValueForConnectionError =
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
    case remoting.ClientSession.ConnectionError.UNKNOWN:
      return 'unknown';
    default:
      return 'unknown-' + connectionError;
  }
};

/** @private */
remoting.ServerLogEntry.KEY_SESSION_DURATION_ = 'session-duration';

/** @private */
remoting.ServerLogEntry.VALUE_EVENT_NAME_CONNECTION_STATISTICS_ =
    "connection-statistics";
/** @private */
remoting.ServerLogEntry.KEY_VIDEO_BANDWIDTH_ = "video-bandwidth";
/** @private */
remoting.ServerLogEntry.KEY_CAPTURE_LATENCY_ = "capture-latency";
/** @private */
remoting.ServerLogEntry.KEY_ENCODE_LATENCY_ = "encode-latency";
/** @private */
remoting.ServerLogEntry.KEY_DECODE_LATENCY_ = "decode-latency";
/** @private */
remoting.ServerLogEntry.KEY_RENDER_LATENCY_ = "render-latency";
/** @private */
remoting.ServerLogEntry.KEY_ROUNDTRIP_LATENCY_ = "roundtrip-latency";

/** @private */
remoting.ServerLogEntry.KEY_OS_NAME_ = 'os-name';
/** @private */
remoting.ServerLogEntry.VALUE_OS_NAME_WINDOWS_ = 'Windows';
/** @private */
remoting.ServerLogEntry.VALUE_OS_NAME_LINUX_ = 'Linux';
/** @private */
remoting.ServerLogEntry.VALUE_OS_NAME_MAC_ = 'Mac';
/** @private */
remoting.ServerLogEntry.VALUE_OS_NAME_CHROMEOS_ = 'ChromeOS';

/** @private */
remoting.ServerLogEntry.KEY_OS_VERSION_ = 'os-version';

/** @private */
remoting.ServerLogEntry.KEY_CPU_ = 'cpu';

/** @private */
remoting.ServerLogEntry.KEY_BROWSER_VERSION_ = 'browser-version';

/** @private */
remoting.ServerLogEntry.KEY_WEBAPP_VERSION_ = 'webapp-version';

/** @private */
remoting.ServerLogEntry.VALUE_EVENT_NAME_SESSION_ID_OLD_ = 'session-id-old';

/** @private */
remoting.ServerLogEntry.VALUE_EVENT_NAME_SESSION_ID_NEW_ = 'session-id-new';

/** @private */
remoting.ServerLogEntry.KEY_MODE_ = 'mode';
/** @private */
remoting.ServerLogEntry.VALUE_MODE_IT2ME_ = 'it2me';
/** @private */
remoting.ServerLogEntry.VALUE_MODE_ME2ME_ = 'me2me';
/** @private */
remoting.ServerLogEntry.VALUE_MODE_UNKNOWN_ = 'unknown';

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
 * Prints this object on the debug log.
 *
 * @param {number} indentLevel the indentation level
 */
remoting.ServerLogEntry.prototype.toDebugLog = function(indentLevel) {
  /** @type Array.<string> */ var fields = [];
  for (var key in this.dict) {
    fields.push(key + ': ' + this.dict[key]);
  }
  console.log(Array(indentLevel+1).join("  ") + fields.join(', '));
};

/**
 * Makes a log entry for a change of client session state.
 *
 * @param {remoting.ClientSession.State} state
 * @param {remoting.ClientSession.ConnectionError} connectionError
 * @param {remoting.ClientSession.Mode} mode
 * @return {remoting.ServerLogEntry}
 */
remoting.ServerLogEntry.makeClientSessionStateChange = function(state,
    connectionError, mode) {
  var entry = new remoting.ServerLogEntry();
  entry.set(remoting.ServerLogEntry.KEY_ROLE_,
            remoting.ServerLogEntry.VALUE_ROLE_CLIENT_);
  entry.set(remoting.ServerLogEntry.KEY_EVENT_NAME_,
            remoting.ServerLogEntry.VALUE_EVENT_NAME_SESSION_STATE_);
  entry.set(remoting.ServerLogEntry.KEY_SESSION_STATE_,
            remoting.ServerLogEntry.getValueForSessionState(state));
  if (connectionError != remoting.ClientSession.ConnectionError.NONE) {
    entry.set(remoting.ServerLogEntry.KEY_CONNECTION_ERROR_,
              remoting.ServerLogEntry.getValueForConnectionError(
                  connectionError));
  }
  entry.addModeField(mode);
  return entry;
};

/**
 * Adds a session duration to a log entry.
 *
 * @param {number} sessionDuration
 */
remoting.ServerLogEntry.prototype.addSessionDurationField = function(
    sessionDuration) {
  this.set(remoting.ServerLogEntry.KEY_SESSION_DURATION_,
      sessionDuration.toString());
};

/**
 * Makes a log entry for a set of connection statistics.
 * Returns null if all the statistics were zero.
 *
 * @param {remoting.StatsAccumulator} statsAccumulator
 * @param {remoting.ClientSession.Mode} mode
 * @return {?remoting.ServerLogEntry}
 */
remoting.ServerLogEntry.makeStats = function(statsAccumulator, mode) {
  var entry = new remoting.ServerLogEntry();
  entry.set(remoting.ServerLogEntry.KEY_ROLE_,
            remoting.ServerLogEntry.VALUE_ROLE_CLIENT_);
  entry.set(remoting.ServerLogEntry.KEY_EVENT_NAME_,
            remoting.ServerLogEntry.VALUE_EVENT_NAME_CONNECTION_STATISTICS_);
  entry.addModeField(mode);
  var nonZero = false;
  nonZero |= entry.addStatsField(
      remoting.ServerLogEntry.KEY_VIDEO_BANDWIDTH_,
      remoting.ClientSession.STATS_KEY_VIDEO_BANDWIDTH, statsAccumulator);
  nonZero |= entry.addStatsField(
      remoting.ServerLogEntry.KEY_CAPTURE_LATENCY_,
      remoting.ClientSession.STATS_KEY_CAPTURE_LATENCY, statsAccumulator);
  nonZero |= entry.addStatsField(
      remoting.ServerLogEntry.KEY_ENCODE_LATENCY_,
      remoting.ClientSession.STATS_KEY_ENCODE_LATENCY, statsAccumulator);
  nonZero |= entry.addStatsField(
      remoting.ServerLogEntry.KEY_DECODE_LATENCY_,
      remoting.ClientSession.STATS_KEY_DECODE_LATENCY, statsAccumulator);
  nonZero |= entry.addStatsField(
      remoting.ServerLogEntry.KEY_RENDER_LATENCY_,
      remoting.ClientSession.STATS_KEY_RENDER_LATENCY, statsAccumulator);
  nonZero |= entry.addStatsField(
      remoting.ServerLogEntry.KEY_ROUNDTRIP_LATENCY_,
      remoting.ClientSession.STATS_KEY_ROUNDTRIP_LATENCY, statsAccumulator);
  if (nonZero) {
    return entry;
  }
  return null;
};

/**
 * Adds one connection statistic to a log entry.
 *
 * @private
 * @param {string} entryKey
 * @param {string} statsKey
 * @param {remoting.StatsAccumulator} statsAccumulator
 * @return {boolean} whether the statistic is non-zero
 */
remoting.ServerLogEntry.prototype.addStatsField = function(
    entryKey, statsKey, statsAccumulator) {
  var val = statsAccumulator.calcMean(statsKey);
  this.set(entryKey, val.toFixed(2));
  return (val != 0);
};

/**
 * Makes a log entry for a "this session ID is old" event.
 *
 * @param {string} sessionId
 * @param {remoting.ClientSession.Mode} mode
 * @return {remoting.ServerLogEntry}
 */
remoting.ServerLogEntry.makeSessionIdOld = function(sessionId, mode) {
  var entry = new remoting.ServerLogEntry();
  entry.set(remoting.ServerLogEntry.KEY_ROLE_,
            remoting.ServerLogEntry.VALUE_ROLE_CLIENT_);
  entry.set(remoting.ServerLogEntry.KEY_EVENT_NAME_,
            remoting.ServerLogEntry.VALUE_EVENT_NAME_SESSION_ID_OLD_);
  entry.addSessionIdField(sessionId);
  entry.addModeField(mode);
  return entry;
};

/**
 * Makes a log entry for a "this session ID is new" event.
 *
 * @param {string} sessionId
 * @param {remoting.ClientSession.Mode} mode
 * @return {remoting.ServerLogEntry}
 */
remoting.ServerLogEntry.makeSessionIdNew = function(sessionId, mode) {
  var entry = new remoting.ServerLogEntry();
  entry.set(remoting.ServerLogEntry.KEY_ROLE_,
            remoting.ServerLogEntry.VALUE_ROLE_CLIENT_);
  entry.set(remoting.ServerLogEntry.KEY_EVENT_NAME_,
            remoting.ServerLogEntry.VALUE_EVENT_NAME_SESSION_ID_NEW_);
  entry.addSessionIdField(sessionId);
  entry.addModeField(mode);
  return entry;
};

/**
 * Adds a session ID field to this log entry.
 *
 * @param {string} sessionId
 */
remoting.ServerLogEntry.prototype.addSessionIdField = function(sessionId) {
  this.set(remoting.ServerLogEntry.KEY_SESSION_ID_, sessionId);
}

/**
 * Adds fields describing the host to this log entry.
 */
remoting.ServerLogEntry.prototype.addHostFields = function() {
  var host = remoting.ServerLogEntry.getHostData();
  if (host) {
    if (host.os_name.length > 0) {
      this.set(remoting.ServerLogEntry.KEY_OS_NAME_, host.os_name);
    }
    if (host.os_version.length > 0) {
      this.set(remoting.ServerLogEntry.KEY_OS_VERSION_, host.os_version);
    }
    if (host.cpu.length > 0) {
      this.set(remoting.ServerLogEntry.KEY_CPU_, host.cpu);
    }
  }
};

/**
 * Extracts host data from the userAgent string.
 *
 * @private
 * @return {{os_name:string, os_version:string, cpu:string} | null}
 */
remoting.ServerLogEntry.getHostData = function() {
  return remoting.ServerLogEntry.extractHostDataFrom(navigator.userAgent);
};

/**
 * Extracts host data from the given userAgent string.
 *
 * @private
 * @param {string} s
 * @return {{os_name:string, os_version:string, cpu:string} | null}
 */
remoting.ServerLogEntry.extractHostDataFrom = function(s) {
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
        'os_name': remoting.ServerLogEntry.VALUE_OS_NAME_WINDOWS_,
        'os_version': match[1],
        'cpu': ''
    };
  }
  match = new RegExp('Linux ([a-zA-Z0-9_]*)').exec(s);
  if (match && (match.length >= 2)) {
    return {
        'os_name': remoting.ServerLogEntry.VALUE_OS_NAME_LINUX_,
        'os_version' : '',
        'cpu': match[1]
    };
  }
  match = new RegExp('([a-zA-Z]*) Mac OS X ([0-9_]*)').exec(s);
  if (match && (match.length >= 3)) {
    return {
        'os_name': remoting.ServerLogEntry.VALUE_OS_NAME_MAC_,
        'os_version': match[2].replace(/_/g, '.'),
        'cpu': match[1]
    };
  }
  match = new RegExp('CrOS ([a-zA-Z0-9]*) ([0-9.]*)').exec(s);
  if (match && (match.length >= 3)) {
    return {
        'os_name': remoting.ServerLogEntry.VALUE_OS_NAME_CHROMEOS_,
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
  var version = remoting.ServerLogEntry.getChromeVersion();
  if (version != null) {
    this.set(remoting.ServerLogEntry.KEY_BROWSER_VERSION_, version);
  }
};

/**
 * Extracts the Chrome version from the userAgent string.
 *
 * @private
 * @return {string | null}
 */
remoting.ServerLogEntry.getChromeVersion = function() {
  return remoting.ServerLogEntry.extractChromeVersionFrom(navigator.userAgent);
};

/**
 * Extracts the Chrome version from the given userAgent string.
 *
 * @private
 * @param {string} s
 * @return {string | null}
 */
remoting.ServerLogEntry.extractChromeVersionFrom = function(s) {
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
  var manifest = chrome.runtime.getManifest();
  if (manifest && manifest.version) {
    this.set(remoting.ServerLogEntry.KEY_WEBAPP_VERSION_, manifest.version);
  }
};

/**
 * Adds a field specifying the mode to this log entry.
 *
 * @param {remoting.ClientSession.Mode} mode
 */
remoting.ServerLogEntry.prototype.addModeField = function(mode) {
  this.set(remoting.ServerLogEntry.KEY_MODE_,
      remoting.ServerLogEntry.getModeField(mode));
};

/**
 * Gets the value of the mode field to be put in a log entry.
 *
 * @private
 * @param {remoting.ClientSession.Mode} mode
 * @return {string}
 */
remoting.ServerLogEntry.getModeField = function(mode) {
  switch(mode) {
    case remoting.ClientSession.Mode.IT2ME:
      return remoting.ServerLogEntry.VALUE_MODE_IT2ME_;
    case remoting.ClientSession.Mode.ME2ME:
      return remoting.ServerLogEntry.VALUE_MODE_ME2ME_;
    default:
      return remoting.ServerLogEntry.VALUE_MODE_UNKNOWN_;
  }
};
