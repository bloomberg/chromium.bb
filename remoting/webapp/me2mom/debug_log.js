// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Module to support logging debug messages.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

/** @constructor */
remoting.DebugLog = function(logElement) {
  this.debugLog = logElement;
}

// Maximum numer of lines to record in the debug log.
// Only the most recent <n> lines are displayed.
var MAX_DEBUG_LOG_SIZE = 1000;

/**
 * Add the given message to the debug log.
 *
 * @param {string} message The debug info to add to the log.
 */
remoting.DebugLog.prototype.log = function(message) {
  // Remove lines from top if we've hit our max log size.
  if (this.debugLog.childNodes.length == MAX_DEBUG_LOG_SIZE) {
    this.debugLog.removeChild(this.debugLog.firstChild);
  }

  // Add the new <p> to the end of the debug log.
  var p = document.createElement('p');
  p.appendChild(document.createTextNode(message));
  this.debugLog.appendChild(p);

  // Scroll to bottom of div
  this.debugLog.scrollTop = this.debugLog.scrollHeight;
}

}());
