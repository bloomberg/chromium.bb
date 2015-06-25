// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @interface
 */
remoting.Logger = function() {};

/**
 * @param {number} totalTime The value of time taken to complete authorization.
 * @return {void} Nothing.
 */
remoting.Logger.prototype.setAuthTotalTime = function(totalTime) {};

/**
 * @param {string} hostVersion Version of the host for current session.
 * @return {void} Nothing.
 */
remoting.Logger.prototype.setHostVersion = function(hostVersion) {};

/**
 * Set the connection type (direct, stun relay).
 *
 * @param {string} connectionType
 */
remoting.Logger.prototype.setConnectionType = function(connectionType) {};

/**
 * @param {remoting.ChromotingEvent.Mode} mode
 */
remoting.Logger.prototype.setLogEntryMode = function(mode) {};


/**
 * @param {remoting.SignalStrategy.Type} strategyType
 * @param {remoting.FallbackSignalStrategy.Progress} progress
 */
remoting.Logger.prototype.logSignalStrategyProgress =
    function(strategyType, progress) {};

/**
 * Logs a client session state change.
 *
 * @param {remoting.ClientSession.State} state
 * @param {!remoting.Error} connectionError
 */
remoting.Logger.prototype.logClientSessionStateChange =
    function(state, connectionError) {};

/**
 * Logs connection statistics.
 * @param {Object<number>} stats The connection statistics
 */
remoting.Logger.prototype.logStatistics = function(stats) {};


/**
 * @return {string} The current session id. This is random GUID, refreshed
 *     every 24hrs.
 */
remoting.Logger.prototype.getSessionId = function() {};

// The maximum age of a session ID, in milliseconds.
remoting.Logger.MAX_SESSION_ID_AGE = 24 * 60 * 60 * 1000;

// The time over which to accumulate connection statistics before logging them
// to the server, in milliseconds.
remoting.Logger.CONNECTION_STATS_ACCUMULATE_TIME = 60 * 1000;

})();