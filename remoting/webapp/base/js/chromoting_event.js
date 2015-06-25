// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `7MM"""Mq.                       `7MM
//   MM   `MM.                        MM
//   MM   ,M9  .gP"Ya   ,6"Yb.   ,M""bMM  `7MMpMMMb.pMMMb.  .gP"Ya
//   MMmmdM9  ,M'   Yb 8)   MM ,AP    MM    MM    MM    MM ,M'   Yb
//   MM  YM.  8M""""""  ,pm9MM 8MI    MM    MM    MM    MM 8M""""""
//   MM   `Mb.YM.    , 8M   MM `Mb    MM    MM    MM    MM YM.    ,
// .JMML. .JMM.`Mbmmd' `Moo9^Yo.`Wbmd"MML..JMML  JMML  JMML.`Mbmmd'
//
// This file defines a JavaScript struct that corresponds to
// logs/proto/chromoting/chromoting_extensions.proto
//
// Please keep the two files in sync!
//

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * The members in this struct is used as the JSON payload in outgoing XHRs
 * so they must match the member definitions in chromoting_extensions.proto.
 *
 * @param {remoting.ChromotingEvent.Type} type
 *
 * @constructor
 * @struct
 */
remoting.ChromotingEvent = function(type) {
  /** @private {remoting.ChromotingEvent.Type} */
  this.type = type;
  /** @private {remoting.ChromotingEvent.Os} */
  this.os;
  /** @private {string} */
  this.os_version;
  /** @private {string} */
  this.browser_version;
  /** @private {string} */
  this.webapp_version;
  /** @type {string} */
  this.host_version;
  /** @private {string} */
  this.cpu;
  /** @private {remoting.ChromotingEvent.SessionState} */
  this.session_state;
  /** @private {remoting.ChromotingEvent.ConnectionType} */
  this.connection_type;
  /** @private {string} */
  this.application_id;
  /** @type {string} */
  this.session_id;
  /** @type {remoting.ChromotingEvent.Role} */
  this.role;
  /** @type {remoting.ChromotingEvent.ConnectionError} */
  this.connection_error;
  /** @type {number} */
  this.session_duration;
  /** @type {number} */
  this.video_bandwidth;
  /** @type {number} */
  this.capture_latency;
  /** @type {number} */
  this.encode_latency;
  /** @type {number} */
  this.decode_latency;
  /** @type {number} */
  this.render_latency;
  /** @type {number} */
  this.roundtrip_latency;
  /** @type {remoting.ChromotingEvent.Mode} */
  this.mode;
  /** @type {remoting.ChromotingEvent.SignalStrategyType} */
  this.signal_strategy_type;
  /** @type {remoting.ChromotingEvent.SignalStrategyProgress} */
  this.signal_strategy_progress;

  this.init_();
};

/** @private */
remoting.ChromotingEvent.prototype.init_ = function() {
  // System Info.
  var systemInfo = remoting.getSystemInfo();
  this.cpu = systemInfo.cpu;
  this.os_version = systemInfo.osVersion;
  if (systemInfo.osName === remoting.Os.WINDOWS) {
    this.os = remoting.ChromotingEvent.Os.WINDOWS;
  } else if (systemInfo.osName === remoting.Os.LINUX) {
    this.os = remoting.ChromotingEvent.Os.LINUX;
  } else if (systemInfo.osName === remoting.Os.MAC) {
    this.os = remoting.ChromotingEvent.Os.MAC;
  } else if (systemInfo.osName === remoting.Os.CHROMEOS) {
    this.os = remoting.ChromotingEvent.Os.CHROMEOS;
  }
  this.browser_version = systemInfo.chromeVersion;

  // App Info.
  this.webapp_version = chrome.runtime.getManifest().version;
  this.application_id = chrome.runtime.id;
};

/**
 * @param {remoting.ClientSession.State} state
 * @param {remoting.Error} error
 */
remoting.ChromotingEvent.prototype.setSessionState = function(state, error) {
  this.connection_error = toConnectionError(error);
  this.session_state = toSessionState(state);
};

/**
 * @param {remoting.SignalStrategy.Type} type
 * @param {remoting.FallbackSignalStrategy.Progress} progress
 */
remoting.ChromotingEvent.prototype.setSignalStategyProgress =
    function(type, progress) {
  this.signal_strategy_progress = toSignalStrategyProgress(progress);
  this.signal_strategy_type = toSignalStrategyType(type);
};

/**
 * @param {string} type
 */
remoting.ChromotingEvent.prototype.setConnectionType = function(type) {
  this.connection_type = toConnectionType(type);
};

/**
 * Strips the member functions from the event and returns a simple JavaScript
 * object.  This object will be used as the JSON content of the XHR request.
 * @return {!Object}
 */
remoting.ChromotingEvent.prototype.serialize = function() {
  return /** @type {!Object} */ (base.deepCopy(this));
};

/**
 * TODO(kelvinp): Consolidate the two enums (crbug.com/504200)
 * @param {remoting.ClientSession.State} state
 * @return {remoting.ChromotingEvent.SessionState}
 */
function toSessionState(state) {
  var SessionState = remoting.ChromotingEvent.SessionState;
  switch(state) {
    case remoting.ClientSession.State.UNKNOWN:
      return SessionState.UNKNOWN;
    case remoting.ClientSession.State.INITIALIZING:
      return SessionState.INITIALIZING;
    case remoting.ClientSession.State.CONNECTING:
      return SessionState.CONNECTING;
    case remoting.ClientSession.State.AUTHENTICATED:
      return SessionState.AUTHENTICATED;
    case remoting.ClientSession.State.CONNECTED:
      return SessionState.CONNECTED;
    case remoting.ClientSession.State.CLOSED:
      return SessionState.CLOSED;
    case remoting.ClientSession.State.FAILED:
      return SessionState.CONNECTION_FAILED;
    case remoting.ClientSession.State.CONNECTION_DROPPED:
      return SessionState.CONNECTION_DROPPED;
    case remoting.ClientSession.State.CONNECTION_CANCELED:
      return SessionState.CONNECTION_CANCELED;
    default:
      throw new Error('Unknown session state :=' + state);
  }
}

/**
 * @param {remoting.Error} error
 * @return {remoting.ChromotingEvent.ConnectionError}
 */
function toConnectionError(error) {
  var ConnectionError = remoting.ChromotingEvent.ConnectionError;
  switch (error.getTag()) {
    case remoting.Error.Tag.NONE:
      return ConnectionError.NONE;
    case remoting.Error.Tag.INVALID_ACCESS_CODE:
      return ConnectionError.INVALID_ACCESS_CODE;
    case remoting.Error.Tag.MISSING_PLUGIN:
      return ConnectionError.MISSING_PLUGIN;
    case remoting.Error.Tag.AUTHENTICATION_FAILED:
      return ConnectionError.AUTHENTICATION_FAILED;
    case remoting.Error.Tag.HOST_IS_OFFLINE:
      return ConnectionError.HOST_OFFLINE;
    case remoting.Error.Tag.INCOMPATIBLE_PROTOCOL:
      return ConnectionError.INCOMPATIBLE_PROTOCOL;
    case remoting.Error.Tag.BAD_PLUGIN_VERSION:
      return ConnectionError.ERROR_BAD_PLUGIN_VERSION;
    case remoting.Error.Tag.NETWORK_FAILURE:
      return ConnectionError.NETWORK_FAILURE;
    case remoting.Error.Tag.HOST_OVERLOAD:
      return ConnectionError.HOST_OVERLOAD;
    case remoting.Error.Tag.P2P_FAILURE:
      return ConnectionError.P2P_FAILURE;
    case remoting.Error.Tag.CLIENT_SUSPENDED:
      return ConnectionError.CLIENT_SUSPENDED;
    case remoting.Error.Tag.UNEXPECTED:
      return ConnectionError.UNEXPECTED;
    default:
      throw new Error('Unknown error Tag :=' + error.getTag());
  }
}

/**
 * @param {remoting.SignalStrategy.Type} type
 * @return {remoting.ChromotingEvent.SignalStrategyType}
 */
function toSignalStrategyType(type) {
  switch (type) {
    case remoting.SignalStrategy.Type.XMPP:
      return remoting.ChromotingEvent.SignalStrategyType.XMPP;
    case remoting.SignalStrategy.Type.XMPP:
      return remoting.ChromotingEvent.SignalStrategyType.WCS;
    default:
      throw new Error('Unknown signal strategy type :=' + type);
  }
}

/**
 * @param {remoting.FallbackSignalStrategy.Progress} progress
 * @return {remoting.ChromotingEvent.SignalStrategyProgress}
 */
function toSignalStrategyProgress(progress) {
  var Progress = remoting.FallbackSignalStrategy.Progress;
  switch (progress) {
    case Progress.SUCCEEDED:
      return remoting.ChromotingEvent.SignalStrategyProgress.SUCCEEDED;
    case Progress.FAILED:
      return remoting.ChromotingEvent.SignalStrategyProgress.FAILED;
    case Progress.TIMED_OUT:
      return remoting.ChromotingEvent.SignalStrategyProgress.TIMED_OUT;
    case Progress.SUCCEEDED_LATE:
      return remoting.ChromotingEvent.SignalStrategyProgress.SUCCEEDED_LATE;
    case Progress.FAILED_LATE:
      return remoting.ChromotingEvent.SignalStrategyProgress.FAILED_LATE;
    default:
      throw new Error('Unknown signal strategy progress :=' + progress);
  }
}

/**
 * @param {string} type
 * @return {remoting.ChromotingEvent.ConnectionType}
 */
function toConnectionType(type) {
  switch (type) {
    case 'direct':
      return remoting.ChromotingEvent.ConnectionType.DIRECT;
    case 'stun':
      return remoting.ChromotingEvent.ConnectionType.STUN;
    case 'relay':
      return remoting.ChromotingEvent.ConnectionType.RELAY;
    default:
      throw new Error('Unknown ConnectionType :=' + type);
  }
}

})();

/**
 * @enum {number}
 */
remoting.ChromotingEvent.Type = {
  SESSION_STATE: 1,
  CONNECTION_STATISTICS: 2,
  SESSION_ID_OLD: 3,
  SESSION_ID_NEW: 4,
  HEARTBEAT: 5,
  HEARTBEAT_REJECTED: 6,
  RESTART: 7,
  HOST_STATUS: 8,
  SIGNAL_STRATEGY_PROGRESS: 9
};

/** @enum {number} */
remoting.ChromotingEvent.Role = {
  CLIENT: 0,
  HOST: 1
};

/** @enum {number} */
remoting.ChromotingEvent.Os = {
  LINUX: 1,
  CHROMEOS: 2,
  MAC: 3,
  WINDOWS: 4,
  OTHER: 5,
  ANDROID: 6,
  IOS: 7
};

/** @enum {number} */
remoting.ChromotingEvent.SessionState = {
  UNKNOWN: 1,
  CREATED: 2,
  BAD_PLUGIN_VERSION: 3,
  UNKNOWN_PLUGIN_ERROR: 4,
  CONNECTING: 5,
  INITIALIZING: 6,
  CONNECTED: 7,
  CLOSED: 8,
  CONNECTION_FAILED: 9,
  UNDEFINED: 10,
  PLUGIN_DISABLED: 11,
  CONNECTION_DROPPED: 12,
  CONNECTION_CANCELED: 13,
  AUTHENTICATED: 14
};

/** @enum {number} */
remoting.ChromotingEvent.ConnectionType = {
  DIRECT: 1,
  STUN: 2,
  RELAY: 3
};

/** @enum {number} */
remoting.ChromotingEvent.ConnectionError = {
  NONE: 1,
  HOST_OFFLINE: 2,
  SESSION_REJECTED: 3,
  INCOMPATIBLE_PROTOCOL: 4,
  NETWORK_FAILURE: 5,
  UNKNOWN_ERROR: 6,
  INVALID_ACCESS_CODE: 7,
  MISSING_PLUGIN: 8,
  AUTHENTICATION_FAILED: 9,
  ERROR_BAD_PLUGIN_VERSION: 10,
  HOST_OVERLOAD: 11,
  P2P_FAILURE: 12,
  UNEXPECTED: 13,
  CLIENT_SUSPENDED: 14
};

/** @enum {number} */
remoting.ChromotingEvent.Mode = {
  IT2ME: 1,
  ME2ME: 2,
  LGAPP: 3
};

/** @enum {number} */
remoting.ChromotingEvent.SignalStrategyType = {
  XMPP: 1,
  WCS: 2
};

/** @enum {number} */
remoting.ChromotingEvent.SignalStrategyProgress = {
  SUCCEEDED: 1,
  FAILED: 2,
  TIMED_OUT: 3,
  SUCCEEDED_LATE: 4,
  FAILED_LATE: 5
};

