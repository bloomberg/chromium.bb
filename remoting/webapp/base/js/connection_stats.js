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
 * @param {remoting.ClientPlugin} plugin
 * @param {remoting.WindowShape=} opt_windowShape
 *
 * @implements {remoting.WindowShape.ClientUI}
 * @implements {base.Disposable}
 */
remoting.ConnectionStats = function(statsElement, plugin, opt_windowShape) {
  /** @private */
  this.statsElement_ = statsElement;

  /** @private {remoting.ClientSession.PerfStats} */
  this.mostRecent_ = null

  /** @private */
  this.plugin_ = plugin;

  var that = this;

  /** @private */
  this.timer_ = new base.RepeatingTimer(function(){
    that.update(plugin.getPerfStats());
  }, 1000, true);

  /** @private */
  this.windowShape_ = opt_windowShape;
  if (this.windowShape_) {
    this.windowShape_.registerClientUI(this);
  }
};

remoting.ConnectionStats.prototype.dispose = function() {
  base.dispose(this.timer_);
  this.timer_ = null;
  this.plugin_ = null;
  if (this.windowShape_) {
    this.windowShape_.unregisterClientUI(this);
  }
};

/**
 * @return {remoting.ClientSession.PerfStats} The most recently-set PerfStats,
 *     or null if update() has not yet been called.
 */
remoting.ConnectionStats.prototype.mostRecent = function() {
  return this.mostRecent_;
};

/**
 * Show or hide the connection stats div.
 */
remoting.ConnectionStats.prototype.toggle = function() {
  this.statsElement_.hidden = !this.statsElement_.hidden;
};

/**
 * @return {boolean}
 */
remoting.ConnectionStats.prototype.isVisible = function() {
  return !this.statsElement_.hidden;
};

/**
 * Show or hide the connection stats div.
 * @param {boolean} show
 */
remoting.ConnectionStats.prototype.show = function(show) {
  this.statsElement_.hidden = !show;
};

/**
 * If the stats panel is visible, add its bounding rectangle to the specified
 * region.
 * @param {Array<{left: number, top: number, width: number, height: number}>}
 *     rects List of rectangles.
 */

remoting.ConnectionStats.prototype.addToRegion = function(rects) {
  if (!this.statsElement_.hidden) {
    rects.push(this.statsElement_.getBoundingClientRect());
  }
};

/**
 * Update the statistics panel.
 * @param {remoting.ClientSession.PerfStats} stats The connection statistics.
 */
remoting.ConnectionStats.prototype.update = function(stats) {
  this.mostRecent_ = stats;
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
  this.statsElement_.innerText = (
      'Bandwidth: ' + formatStatNumber(videoBandwidth, units) +
      ', Frame Rate: ' + formatStatNumber(stats.videoFrameRate, 'fps') +
      ', Capture: ' + formatStatNumber(stats.captureLatency, 'ms') +
      ', Encode: ' + formatStatNumber(stats.encodeLatency, 'ms') +
      ', Decode: ' + formatStatNumber(stats.decodeLatency, 'ms') +
      ', Render: ' + formatStatNumber(stats.renderLatency, 'ms') +
      ', Latency: ' + formatStatNumber(stats.roundtripLatency, 'ms'));
};
