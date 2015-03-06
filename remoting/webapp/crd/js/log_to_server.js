// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
 * @param {remoting.SignalStrategy} signalStrategy Signal strategy.
 * @param {remoting.DesktopConnectedView.Mode} mode The mode of this connection.
 * @constructor
 */
remoting.LogToServer = function(signalStrategy, mode) {
  /** @private */
  this.statsAccumulator_ = new remoting.StatsAccumulator();
  /** @private */
  this.sessionId_ = '';
  /** @private */
  this.sessionIdGenerationTime_ = 0;
  /** @private */
  this.sessionStartTime_ = new Date().getTime();
  /** @private */
  this.signalStrategy_ = signalStrategy;
  /** @private */
  this.mode_ = mode;
  /** @private {string} */
  this.connectionType_ = '';

  this.setSessionId_();
  signalStrategy.sendConnectionSetupResults(this);
};

// Constants used for generating a session ID.
/** @private */
remoting.LogToServer.SESSION_ID_ALPHABET_ =
    'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890';
/** @private */
remoting.LogToServer.SESSION_ID_LEN_ = 20;

// The maximum age of a session ID, in milliseconds.
remoting.LogToServer.MAX_SESSION_ID_AGE = 24 * 60 * 60 * 1000;

// The time over which to accumulate connection statistics before logging them
// to the server, in milliseconds.
remoting.LogToServer.CONNECTION_STATS_ACCUMULATE_TIME = 60 * 1000;

/**
 * Logs a client session state change.
 *
 * @param {remoting.ClientSession.State} state
 * @param {remoting.Error} connectionError
 */
remoting.LogToServer.prototype.logClientSessionStateChange =
    function(state, connectionError) {
  this.maybeExpireSessionId_();
  // Log the session state change.
  var entry = remoting.ServerLogEntry.makeClientSessionStateChange(
      state, connectionError, this.mode_);
  entry.addHostFields();
  entry.addChromeVersionField();
  entry.addWebappVersionField();
  entry.addSessionIdField(this.sessionId_);
  this.log_(entry);
  // Don't accumulate connection statistics across state changes.
  this.logAccumulatedStatistics_();
  this.statsAccumulator_.empty();
  // Maybe clear the session ID.
  if (remoting.LogToServer.isEndOfSession_(state)) {
    this.clearSessionId_();
  }
};

/**
 * Set the connection type (direct, stun relay).
 *
 * @param {string} connectionType
 */
remoting.LogToServer.prototype.setConnectionType = function(connectionType) {
  this.connectionType_ = connectionType;
};

/**
 * @param {remoting.SignalStrategy.Type} strategyType
 * @param {remoting.FallbackSignalStrategy.Progress} progress
 */
remoting.LogToServer.prototype.logSignalStrategyProgress =
    function(strategyType, progress) {
  this.maybeExpireSessionId_();
  var entry = remoting.ServerLogEntry.makeSignalStrategyProgress(
      this.sessionId_, strategyType, progress);
  this.log_(entry);
};

/**
 * Whether a session state is one of the states that occurs at the start of
 * a session.
 *
 * @private
 * @param {remoting.ClientSession.State} state
 * @return {boolean}
 */
remoting.LogToServer.isStartOfSession_ = function(state) {
  return ((state == remoting.ClientSession.State.CONNECTING) ||
      (state == remoting.ClientSession.State.INITIALIZING) ||
      (state == remoting.ClientSession.State.CONNECTED));
};

/**
 * Whether a session state is one of the states that occurs at the end of
 * a session.
 *
 * @private
 * @param {remoting.ClientSession.State} state
 * @return {boolean}
 */
remoting.LogToServer.isEndOfSession_ = function(state) {
  return ((state == remoting.ClientSession.State.CLOSED) ||
      (state == remoting.ClientSession.State.FAILED) ||
      (state == remoting.ClientSession.State.CONNECTION_DROPPED) ||
      (state == remoting.ClientSession.State.CONNECTION_CANCELED));
};


/**
 * Logs connection statistics.
 * @param {Object<string, number>} stats The connection statistics
 */
remoting.LogToServer.prototype.logStatistics = function(stats) {
  this.maybeExpireSessionId_();
  // Store the statistics.
  this.statsAccumulator_.add(stats);
  // Send statistics to the server if they've been accumulating for at least
  // 60 seconds.
  if (this.statsAccumulator_.getTimeSinceFirstValue() >=
      remoting.LogToServer.CONNECTION_STATS_ACCUMULATE_TIME) {
    this.logAccumulatedStatistics_();
  }
};

/**
 * Moves connection statistics from the accumulator to the log server.
 *
 * If all the statistics are zero, then the accumulator is still emptied,
 * but the statistics are not sent to the log server.
 *
 * @private
 */
remoting.LogToServer.prototype.logAccumulatedStatistics_ = function() {
  var entry = remoting.ServerLogEntry.makeStats(this.statsAccumulator_,
                                                this.connectionType_,
                                                this.mode_);
  if (entry) {
    entry.addHostFields();
    entry.addChromeVersionField();
    entry.addWebappVersionField();
    entry.addSessionIdField(this.sessionId_);
    this.log_(entry);
  }
  this.statsAccumulator_.empty();
};

/**
 * Sends a log entry to the server.
 *
 * @private
 * @param {remoting.ServerLogEntry} entry
 */
remoting.LogToServer.prototype.log_ = function(entry) {
  // Log the time taken to get to this point from the time this session started.
  var sessionDurationInSeconds =
      (new Date().getTime() - this.sessionStartTime_) / 1000.0;
  entry.addSessionDuration(sessionDurationInSeconds);

  // Send the stanza to the debug log.
  console.log('Enqueueing log entry:');
  entry.toDebugLog(1);

  var stanza = '<cli:iq to="' + remoting.settings.DIRECTORY_BOT_JID + '" ' +
                       'type="set" xmlns:cli="jabber:client">' +
                 '<gr:log xmlns:gr="google:remoting">' +
                   entry.toStanza() +
                 '</gr:log>' +
               '</cli:iq>';
  this.signalStrategy_.sendMessage(stanza);
};

/**
 * Sets the session ID to a random string.
 *
 * @private
 */
remoting.LogToServer.prototype.setSessionId_ = function() {
  this.sessionId_ = remoting.LogToServer.generateSessionId_();
  this.sessionIdGenerationTime_ = new Date().getTime();
};

/**
 * Clears the session ID.
 *
 * @private
 */
remoting.LogToServer.prototype.clearSessionId_ = function() {
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
remoting.LogToServer.prototype.maybeExpireSessionId_ = function() {
  if ((this.sessionId_ != '') &&
      (new Date().getTime() - this.sessionIdGenerationTime_ >=
      remoting.LogToServer.MAX_SESSION_ID_AGE)) {
    // Log the old session ID.
    var entry = remoting.ServerLogEntry.makeSessionIdOld(this.sessionId_,
                                                         this.mode_);
    this.log_(entry);
    // Generate a new session ID.
    this.setSessionId_();
    // Log the new session ID.
    entry = remoting.ServerLogEntry.makeSessionIdNew(this.sessionId_,
                                                     this.mode_);
    this.log_(entry);
  }
};

/**
 * Generates a string that can be used as a session ID.
 *
 * @private
 * @return {string} a session ID
 */
remoting.LogToServer.generateSessionId_ = function() {
  var idArray = [];
  for (var i = 0; i < remoting.LogToServer.SESSION_ID_LEN_; i++) {
    var index =
        Math.random() * remoting.LogToServer.SESSION_ID_ALPHABET_.length;
    idArray.push(
        remoting.LogToServer.SESSION_ID_ALPHABET_.slice(index, index + 1));
  }
  return idArray.join('');
};
