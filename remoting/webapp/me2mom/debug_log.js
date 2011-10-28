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

/**
 * @constructor
 * @param {Element} logElement The HTML div to which to add log messages.
 * @param {Element} statsElement The HTML div to which to update stats.
 */
remoting.DebugLog = function(logElement, statsElement) {
  this.logElement = logElement;
  this.statsElement = statsElement;
};

/**
 * Maximum number of lines to record in the debug log. Only the most
 * recent <n> lines are displayed.
 */
remoting.DebugLog.prototype.MAX_DEBUG_LOG_SIZE = 1000;

/**
 * Add the given message to the debug log.
 *
 * @param {string} message The debug info to add to the log.
 */
remoting.DebugLog.prototype.log = function(message) {
  // Remove lines from top if we've hit our max log size.
  if (this.logElement.childNodes.length == this.MAX_DEBUG_LOG_SIZE) {
    this.logElement.removeChild(this.logElement.firstChild);
  }

  // Add the new <p> to the end of the debug log.
  var p = document.createElement('p');
  p.appendChild(document.createTextNode(message));
  this.logElement.appendChild(p);

  // Scroll to bottom of div
  this.logElement.scrollTop = this.logElement.scrollHeight;
};

/**
 * Show or hide the debug log.
 */
remoting.DebugLog.prototype.toggle = function() {
  var debugLog = /** @type {Element} */ this.logElement.parentNode;
  if (debugLog.hidden) {
    debugLog.hidden = false;
  } else {
    debugLog.hidden = true;
  }
};

/**
 * Update the statistics panel.
 * @param {Object.<string, number>} stats The connection statistics.
 */
remoting.DebugLog.prototype.updateStatistics = function(stats) {
  var units = '';
  var videoBandwidth = stats['video_bandwidth'];
  if (videoBandwidth < 1024) {
    units = 'Bps';
  } else if (videoBandwidth < 1048576) {
    units = 'KiBps';
    videoBandwidth = videoBandwidth / 1024;
  } else if (videoBandwidth < 1073741824) {
    units = 'MiBps';
    videoBandwidth = videoBandwidth / 1048576;
  } else {
    units = 'GiBps';
    videoBandwidth = videoBandwidth / 1073741824;
  }

  var statistics = document.getElementById('statistics');
  this.statsElement.innerText =
      'Bandwidth: ' + videoBandwidth.toFixed(2) + units +
      ', Frame Rate: ' +
          (stats['video_frame_rate'] ?
           stats['video_frame_rate'].toFixed(2) + ' fps' : 'n/a') +
      ', Capture: ' + stats['capture_latency'].toFixed(2) + 'ms' +
      ', Encode: ' + stats['encode_latency'].toFixed(2) + 'ms' +
      ', Decode: ' + stats['decode_latency'].toFixed(2) + 'ms' +
      ', Render: ' + stats['render_latency'].toFixed(2) + 'ms' +
      ', Latency: ' + stats['roundtrip_latency'].toFixed(2) + 'ms';
};

/**
 * Check for the debug toggle hot-key.
 *
 * @param {Event} event The keyboard event.
 * @return {void} Nothing.
 */
remoting.DebugLog.onKeydown = function(event) {
  if (String.fromCharCode(event.which) == 'D') {
    remoting.debug.toggle();
  }
}

/** @type {remoting.DebugLog} */
remoting.debug = null;
