// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * |remoting.SessionLogger| is responsible for reporting telemetry entries for
 * a Chromoting session.
 *
 * @param {remoting.ChromotingEvent.Role} role
 * @param {function(!Object)} writeLogEntry
 *
 * @constructor
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
  this.sessionStartTime_ = Date.now();
  /** @private */
  this.sessionEndTime_ = 0;
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
  this.hostOs_ = remoting.ChromotingEvent.Os.OTHER;
  /** @private */
  this.hostOsVersion_ = '';
  /** @private {number} */
  this.hostStatusUpdateElapsedTime_;
  /** @private */
  this.mode_ = remoting.ChromotingEvent.Mode.ME2ME;
  /** @private {remoting.ChromotingEvent.AuthMethod} */
  this.authMethod_;
  /** @private {remoting.ChromotingEvent} */
  this.lastSessionEntry_ = null;
  /** @private {remoting.ChromotingEvent.SessionSummary} */
  this.previousSessionSummary_ = null;

  this.setSessionId_();
};

/**
 * @param {remoting.ChromotingEvent.SessionEntryPoint} entryPoint
 */
remoting.SessionLogger.prototype.setEntryPoint = function(entryPoint) {
  this.entryPoint_ = entryPoint;
};

/**
 * @param {number} totalTime The value of time taken to complete authorization.
 * @return {void} Nothing.
 */
remoting.SessionLogger.prototype.setAuthTotalTime = function(totalTime) {
  this.authTotalTime_ = totalTime;
};

/**
 * @param {string} hostVersion Version of the host for current session.
 * @return {void} Nothing.
 */
remoting.SessionLogger.prototype.setHostVersion = function(hostVersion) {
  this.hostVersion_ = hostVersion;
};

/**
 * @param {remoting.ChromotingEvent.Os} hostOs Type of the OS the host
 *        for the current session.
 * @return {void} Nothing.
 */
remoting.SessionLogger.prototype.setHostOs = function(hostOs) {
  this.hostOs_ = hostOs;
};

/**
 * @param {string} hostOsVersion Version of the host Os for current session.
 * @return {void} Nothing.
 */
remoting.SessionLogger.prototype.setHostOsVersion = function(hostOsVersion) {
  this.hostOsVersion_ = hostOsVersion;
};

/**
 * @param {number} time Time in milliseconds since the last host status update.
 * @return {void} Nothing.
 */
remoting.SessionLogger.prototype.setHostStatusUpdateElapsedTime =
    function(time) {
  this.hostStatusUpdateElapsedTime_ = time;
};

/**
 * Set the connection type (direct, stun relay).
 *
 * @param {string} connectionType
 */
remoting.SessionLogger.prototype.setConnectionType = function(connectionType) {
  this.connectionType_ = toConnectionType(connectionType);
};

/**
 * @param {remoting.ChromotingEvent.Mode} mode
 */
remoting.SessionLogger.prototype.setLogEntryMode = function(mode) {
  this.mode_ = mode;
};

/**
 * @param {remoting.ChromotingEvent.AuthMethod} method
 */
remoting.SessionLogger.prototype.setAuthMethod = function(method) {
  this.authMethod_ = method;
};

/**
 * @param {remoting.ChromotingEvent.SessionSummary} summary
 */
remoting.SessionLogger.prototype.setPreviousSessionSummary =
    function(summary) {
  this.previousSessionSummary_ = summary;
};

/**
 * @return {string} The current session id. This is random GUID, refreshed
 *     every 24hrs.
 */
remoting.SessionLogger.prototype.getSessionId = function() {
  return this.sessionId_;
};

/**
 * @return {remoting.ChromotingEvent.SessionSummary} A snapshot of the current
 *     session.
 */
remoting.SessionLogger.prototype.createSummary = function() {
  var summary = new remoting.ChromotingEvent.SessionSummary();
  summary.session_id = this.lastSessionEntry_.session_id;
  summary.last_state = this.lastSessionEntry_.session_state;
  summary.last_error = this.lastSessionEntry_.connection_error;
  summary.entry_point = this.lastSessionEntry_.session_entry_point;
  summary.duration = this.lastSessionEntry_.session_duration;
  if (this.sessionEndTime_ > 0) {
    summary.session_end_elapsed_time =
        (Date.now() - this.sessionEndTime_) / 1000;
  }
  return summary;
};

/**
 * @param {remoting.SignalStrategy.Type} strategyType
 * @param {remoting.FallbackSignalStrategy.Progress} progress
 */
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

/**
 * @param {remoting.ChromotingEvent.SessionState} state
 * @param {remoting.Error=} opt_error
 */
remoting.SessionLogger.prototype.logSessionStateChange =
    function(state, opt_error) {
  this.maybeExpireSessionId_();

  var entry = this.makeSessionStateChange_(state, opt_error);
  entry.previous_session_state = this.previousSessionState_;
  this.previousSessionState_ = state;

  this.log_(entry);

  this.lastSessionEntry_ =
      /** @type {remoting.ChromotingEvent} */ (base.deepCopy(entry));

  // Update the session summary.
  if (remoting.ChromotingEvent.isEndOfSession(entry)) {
    this.sessionEndTime_ = Date.now();
  }

  // Don't accumulate connection statistics across state changes.
  this.logAccumulatedStatistics_();
  this.statsAccumulator_.empty();
};

/**
 * Logs connection statistics.
 *
 * @param {Object<number>} stats The connection statistics
 */
remoting.SessionLogger.prototype.logStatistics = function(stats) {
  this.maybeExpireSessionId_();
  // Store the statistics.
  this.statsAccumulator_.add(stats);
  // Send statistics to the server if they've been accumulating for at least
  // 60 seconds.
  if (this.statsAccumulator_.getTimeSinceFirstValue() >=
      remoting.SessionLogger.CONNECTION_STATS_ACCUMULATE_TIME) {
    this.logAccumulatedStatistics_();
  }
};

/**
 * @param {remoting.ChromotingEvent.SessionState} state
 * @param {remoting.Error=} opt_error
 * @return {remoting.ChromotingEvent}
 * @private
 */
remoting.SessionLogger.prototype.makeSessionStateChange_ =
    function(state, opt_error) {
  var entry = new remoting.ChromotingEvent(
      remoting.ChromotingEvent.Type.SESSION_STATE);

  var ConnectionError = remoting.ChromotingEvent.ConnectionError;

  if (!opt_error) {
    entry.connection_error = ConnectionError.NONE;
  } else if (opt_error instanceof remoting.Error) {
    entry.setError(opt_error);
  } else {
    entry.connection_error = ConnectionError.UNKNOWN_ERROR;
  }

  entry.session_state = state;

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
  if (this.authMethod_ != undefined) {
    entry.auth_method = this.authMethod_;
  }
  if (this.previousSessionSummary_) {
    entry.previous_session = this.previousSessionSummary_;
  }
  entry.host_version = this.hostVersion_;
  entry.host_os = this.hostOs_;
  entry.host_os_version = this.hostOsVersion_;
};

/**
 * Sends a log entry to the server.
 *
 * @param {remoting.ChromotingEvent} entry
 * @private
 */
remoting.SessionLogger.prototype.log_ = function(entry) {
  this.writeLogEntry_(/** @type {!Object} */ (base.deepCopy(entry)));
};

/**
 * Sets the session ID to a random string.
 *
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
      remoting.SessionLogger.MAX_SESSION_ID_AGE)) {
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

// The maximum age of a session ID, in milliseconds.
remoting.SessionLogger.MAX_SESSION_ID_AGE = 24 * 60 * 60 * 1000;

// The time over which to accumulate connection statistics before logging them
// to the server, in milliseconds.
remoting.SessionLogger.CONNECTION_STATS_ACCUMULATE_TIME = 60 * 1000;

})();
