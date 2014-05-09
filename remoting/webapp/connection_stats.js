// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Module to support debug overlay window with connection stats.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {Element} statsElement The HTML div to which to update stats.
 */
remoting.ConnectionStats = function(statsElement) {
  this.statsElement = statsElement;
};

/**
 * Show or hide the connection stats div.
 */
remoting.ConnectionStats.prototype.toggle = function() {
  this.statsElement.hidden = !this.statsElement.hidden;
};

/**
 * Update the statistics panel.
 * @param {remoting.ClientSession.PerfStats} stats The connection statistics.
 */
remoting.ConnectionStats.prototype.update = function(stats) {
  var units = '';
  var videoBandwidth = stats.videoBandwidth;
  if (videoBandwidth != undefined) {
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
  }

  /**
   * @param {number} value
   * @param {string} units
   * @return {string} Formatted number.
   */
  function formatStatNumber(value, units) {
    if (value != undefined) {
      return value.toFixed(2) + ' ' + units;
    } else {
      return "n/a";
    }
  }

  var statistics = document.getElementById('statistics');
  this.statsElement.innerText = (
      'Bandwidth: ' + formatStatNumber(videoBandwidth, units) +
      ', Frame Rate: ' + formatStatNumber(stats.videoFrameRate, 'fps') +
      ', Capture: ' + formatStatNumber(stats.captureLatency, 'ms') +
      ', Encode: ' + formatStatNumber(stats.encodeLatency, 'ms') +
      ', Decode: ' + formatStatNumber(stats.decodeLatency, 'ms') +
      ', Render: ' + formatStatNumber(stats.renderLatency, 'ms') +
      ', Latency: ' + formatStatNumber(stats.roundtripLatency, 'ms'));
};

/**
 * Check for the debug toggle hot-key.
 *
 * @param {Event} event The keyboard event.
 * @return {void} Nothing.
 */
remoting.ConnectionStats.onKeydown = function(event) {
  var element = /** @type {Element} */ (event.target);
  if (element.tagName == 'INPUT' || element.tagName == 'TEXTAREA') {
    return;
  }
  if (String.fromCharCode(event.which) == 'D') {
    remoting.stats.toggle();
  }
};

/** @type {remoting.ConnectionStats} */
remoting.stats = null;
