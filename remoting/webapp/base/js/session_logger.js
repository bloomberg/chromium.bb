// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * This class defines a remoting.Logger implementation that generates
 * log entries in the format of remoting.ChromotingEvent.
 *
 * @param {remoting.ChromotingEvent.Role} role
 * @param {function(!Object)} writeLogEntry
 *
 * @constructor
 * @implements {remoting.Logger}
 */
remoting.SessionLogger = function(role, writeLogEntry) {
  /** @private */
  this.role_ = role;
  /** @private */
  this.writeLogEntry_ = writeLogEntry;
  /** @private */
  this.statsAccumulator_ = new remoting.StatsAccumulator();
  /** @private */
  this.sessionId_ = '';
  /** @private */
  this.sessionIdGenerationTime_ = 0;
  /** @private */
  this.sessionStartTime_ = new Date().getTime();
  /** @private {remoting.ChromotingEvent.ConnectionType} */
  this.connectionType_;
  /** @private {remoting.ChromotingEvent.SessionEntryPoint} */
  this.entryPoint_;
  /** @private {remoting.ChromotingEvent.SessionState} */
  this.previousSessionState_;
  /** @private */
  this.authTotalTime_ = 0;
  /** @private */
  this.hostVersion_ = '';
  /** @private {remoting.ChromotingEvent.Os}*/
  this.hostOS_ = remoting.ChromotingEvent.Os.OTHER;
  /** @private */
  this.hostOSVersion_ = '';
  /** @private {number} */
  this.hostStatusUpdateElapsedTime_;
  /** @private */
  this.mode_ = remoting.ChromotingEvent.Mode.ME2ME;

  this.setSessionId_();
};

/**
 * @param {remoting.ChromotingEvent.SessionEntryPoint} entryPoint
 */
remoting.SessionLogger.prototype.setEntryPoint = function(entryPoint) {
  this.entryPoint_ = entryPoint;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.setAuthTotalTime = function(totalTime) {
  this.authTotalTime_ = totalTime;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.setHostVersion = function(hostVersion) {
  this.hostVersion_ = hostVersion;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.setHostOS = function(hostOS) {
  this.hostOS_ = hostOS;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.setHostStatusUpdateElapsedTime =
    function(time) {
  this.hostStatusUpdateElapsedTime_ = time;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.setHostOSVersion = function(hostOSVersion) {
  this.hostOSVersion_ = hostOSVersion;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.setConnectionType = function(connectionType) {
  this.connectionType_ = toConnectionType(connectionType);
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.setLogEntryMode = function(mode) {
  this.mode_ = mode;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.getSessionId = function() {
  return this.sessionId_;
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.logSignalStrategyProgress =
    function(strategyType, progress) {
  this.maybeExpireSessionId_();
  var entry = new remoting.ChromotingEvent(
      remoting.ChromotingEvent.Type.SIGNAL_STRATEGY_PROGRESS);
  entry.signal_strategy_type = toSignalStrategyType(strategyType);
  entry.signal_strategy_progress = toSignalStrategyProgress(progress);

  this.fillEvent_(entry);
  this.log_(entry);
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.logClientSessionStateChange = function(
    state, connectionError, xmppError) {
  this.logSessionStateChange(
      toSessionState(state),
      toConnectionError(connectionError),
      xmppError);
};

/**
 * @param {remoting.ChromotingEvent.SessionState} state
 * @param {remoting.ChromotingEvent.ConnectionError} error
 * @param {remoting.ChromotingEvent.XmppError=} opt_XmppError
 */
remoting.SessionLogger.prototype.logSessionStateChange = function(
    state, error, opt_XmppError) {
  this.maybeExpireSessionId_();

  var entry = this.makeSessionStateChange_(
      state, error,
      /** @type {?remoting.ChromotingEvent.XmppError} */ (opt_XmppError));
  entry.previous_session_state = this.previousSessionState_;
  this.previousSessionState_ = state;

  this.log_(entry);

  // Don't accumulate connection statistics across state changes.
  this.logAccumulatedStatistics_();
  this.statsAccumulator_.empty();
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.logStatistics = function(stats) {
  this.maybeExpireSessionId_();
  // Store the statistics.
  this.statsAccumulator_.add(stats);
  // Send statistics to the server if they've been accumulating for at least
  // 60 seconds.
  if (this.statsAccumulator_.getTimeSinceFirstValue() >=
      remoting.Logger.CONNECTION_STATS_ACCUMULATE_TIME) {
    this.logAccumulatedStatistics_();
  }
};

/**
 * @param {remoting.ChromotingEvent.SessionState} state
 * @param {remoting.ChromotingEvent.ConnectionError} error
 * @param {?remoting.ChromotingEvent.XmppError} xmppError
 * @return {remoting.ChromotingEvent}
 * @private
 */
remoting.SessionLogger.prototype.makeSessionStateChange_ =
    function(state, error, xmppError) {
  var entry = new remoting.ChromotingEvent(
      remoting.ChromotingEvent.Type.SESSION_STATE);
  entry.connection_error = error;
  entry.session_state = state;

  if (Boolean(xmppError)) {
    entry.xmpp_error = xmppError;
  }

  this.fillEvent_(entry);
  return entry;
};

/**
 * @return {remoting.ChromotingEvent}
 * @private
 */
remoting.SessionLogger.prototype.makeSessionIdNew_ = function() {
  var entry = new remoting.ChromotingEvent(
      remoting.ChromotingEvent.Type.SESSION_ID_NEW);
  this.fillEvent_(entry);
  return entry;
};

/**
 * @return {remoting.ChromotingEvent}
 * @private
 */
remoting.SessionLogger.prototype.makeSessionIdOld_ = function() {
  var entry = new remoting.ChromotingEvent(
      remoting.ChromotingEvent.Type.SESSION_ID_OLD);
  this.fillEvent_(entry);
  return entry;
};

/**
 * @return {remoting.ChromotingEvent}
 * @private
 */
remoting.SessionLogger.prototype.makeStats_ = function() {
  var perfStats = this.statsAccumulator_.getPerfStats();
  if (Boolean(perfStats)) {
    var entry = new remoting.ChromotingEvent(
        remoting.ChromotingEvent.Type.CONNECTION_STATISTICS);
    this.fillEvent_(entry);
    entry.video_bandwidth = perfStats.videoBandwidth;
    entry.capture_latency = perfStats.captureLatency;
    entry.encode_latency = perfStats.encodeLatency;
    entry.decode_latency = perfStats.decodeLatency;
    entry.render_latency = perfStats.renderLatency;
    entry.roundtrip_latency = perfStats.roundtripLatency;
    return entry;
  }
  return null;
};

/**
 * Moves connection statistics from the accumulator to the log server.
 *
 * If all the statistics are zero, then the accumulator is still emptied,
 * but the statistics are not sent to the log server.
 *
 * @private
 */
remoting.SessionLogger.prototype.logAccumulatedStatistics_ = function() {
  var entry = this.makeStats_();
  if (entry) {
    this.log_(entry);
  }
  this.statsAccumulator_.empty();
};

/**
 * @param {remoting.ChromotingEvent} entry
 * @private
 */
remoting.SessionLogger.prototype.fillEvent_ = function(entry) {
  entry.session_id = this.sessionId_;
  entry.mode = this.mode_;
  entry.role = this.role_;
  entry.session_entry_point = this.entryPoint_;
  var sessionDurationInSeconds =
      (new Date().getTime() - this.sessionStartTime_ -
          this.authTotalTime_) / 1000.0;
  entry.session_duration = sessionDurationInSeconds;
  if (Boolean(this.connectionType_)) {
    entry.connection_type = this.connectionType_;
  }
  if (this.hostStatusUpdateElapsedTime_ != undefined) {
    entry.host_status_update_elapsed_time = this.hostStatusUpdateElapsedTime_;
  }
  entry.host_version = this.hostVersion_;
  entry.host_os = this.hostOS_;
  entry.host_os_version = this.hostOSVersion_;
};

/**
 * Sends a log entry to the server.
 * @param {remoting.ChromotingEvent} entry
 * @private
 */
remoting.SessionLogger.prototype.log_ = function(entry) {
  this.writeLogEntry_(/** @type {!Object} */ (base.deepCopy(entry)));
};

/**
 * Sets the session ID to a random string.
 * @private
 */
remoting.SessionLogger.prototype.setSessionId_ = function() {
  var random = new Uint8Array(20);
  window.crypto.getRandomValues(random);
  this.sessionId_ = window.btoa(String.fromCharCode.apply(null, random));
  this.sessionIdGenerationTime_ = new Date().getTime();
};

/**
 * Clears the session ID.
 *
 * @private
 */
remoting.SessionLogger.prototype.clearSessionId_ = function() {
  this.sessionId_ = '';
  this.sessionIdGenerationTime_ = 0;
};

/**
 * Sets a new session ID, if the current session ID has reached its maximum age.
 *
 * This method also logs the old and new session IDs to the server, in separate
 * log entries.
 *
 * @private
 */
remoting.SessionLogger.prototype.maybeExpireSessionId_ = function() {
  if ((this.sessionId_ !== '') &&
      (new Date().getTime() - this.sessionIdGenerationTime_ >=
      remoting.Logger.MAX_SESSION_ID_AGE)) {
    // Log the old session ID.
    var entry = this.makeSessionIdOld_();
    this.log_(entry);
    // Generate a new session ID.
    this.setSessionId_();
    // Log the new session ID.
    entry = this.makeSessionIdNew_();
    this.log_(entry);
  }
};

/** @return {remoting.SessionLogger} */
remoting.SessionLogger.createForClient = function() {
  return new remoting.SessionLogger(remoting.ChromotingEvent.Role.CLIENT,
                                    remoting.TelemetryEventWriter.Client.write);
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
      throw new Error('Unknown session state : ' + state);
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
    case remoting.Error.Tag.NACL_DISABLED:
      return ConnectionError.NACL_DISABLED;
    default:
      throw new Error('Unknown error Tag : ' + error.getTag());
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
    case remoting.SignalStrategy.Type.WCS:
      return remoting.ChromotingEvent.SignalStrategyType.WCS;
    default:
      throw new Error('Unknown signal strategy type : ' + type);
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
