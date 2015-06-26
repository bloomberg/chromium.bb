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
  /** @private  */
  this.connectionType_ = '';
  /** @private */
  this.authTotalTime_ = 0;
  /** @private */
  this.hostVersion_ = '';
  /** @private */
  this.mode_ = remoting.ChromotingEvent.Mode.ME2ME;

  this.setSessionId_();
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
remoting.SessionLogger.prototype.setConnectionType = function(connectionType) {
  this.connectionType_ = connectionType;
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
  entry.setSignalStategyProgress(strategyType, progress);
  this.fillEvent_(entry);
  this.log_(entry);
};

/** @override {remoting.Logger} */
remoting.SessionLogger.prototype.logClientSessionStateChange =
    function(state, connectionError) {
  this.maybeExpireSessionId_();
  // Log the session state change.
  var entry = this.makeSessionStateChange_(state, connectionError);
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
 * @param {remoting.ClientSession.State} state
 * @param {remoting.Error} error
 * @return {remoting.ChromotingEvent}
 * @private
 */
remoting.SessionLogger.prototype.makeSessionStateChange_ =
    function(state, error) {
  var entry = new remoting.ChromotingEvent(
      remoting.ChromotingEvent.Type.SESSION_STATE);
  entry.setSessionState(state, error);
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
  var sessionDurationInSeconds =
      (new Date().getTime() - this.sessionStartTime_ -
          this.authTotalTime_) / 1000.0;
  entry.session_duration = sessionDurationInSeconds;
  if (Boolean(this.connectionType_)) {
    entry.setConnectionType(this.connectionType_);
  }
  entry.host_version = this.hostVersion_;
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

})();