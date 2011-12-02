// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class representing the client tool-bar.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Element} toolbar The HTML element representing the tool-bar.
 * @constructor
 */
remoting.Toolbar = function(toolbar) {
  /**
   * @type {Element}
   * @private
   */
  this.toolbar_ = toolbar;
  /** @type {remoting.Toolbar} */
  var that = this;

  /**
   * @param {Event} event The mouseout event, used to determine whether or
   *     not the mouse is leaving the tool-bar or (due to event-bubbling)
   *     one of its children.
   */
  var onMouseOut = function(event) {
    for (var e = event.toElement; e != null; e = e.parentElement) {
      if (e == that.toolbar_) {
        return;  // Still over a child element => ignore.
      }
    }
    that.hide_();
  };
  this.toolbar_.onmouseout = onMouseOut;

  this.toolbar_.onmouseover = function() {
    that.showForAtLeast_(1000);
  };

  /**
   * @type {boolean} False if the tool-bar is currently hidden, or should be
   *      hidden once the over-shoot timer expires; true otherwise.
   */
  this.visible = false;
  /**
   * @type {number?} The id of the current timer, if any.
   */
  this.timerId = null;
};

/**
 * Preview the tool-bar functionality by showing it for 3s if it is not
 *     already visible.
 * @return {void} Nothing.
 */
remoting.Toolbar.prototype.preview = function() {
  this.showForAtLeast_(3000);
  this.visible = false;
};

/**
 * Center the tool-bar horizonally.
 */
remoting.Toolbar.prototype.center = function() {
  var toolbarX = (window.innerWidth - this.toolbar_.clientWidth) / 2;
  this.toolbar_.style['left'] = toolbarX + 'px';
};

/**
 * If the tool-bar is not currently visible, show it and start a timer to
 * prevent it from being hidden again for a short time. This is to guard
 * against users over-shooting the tool-bar stub when trying to access it.
 *
 * @param {number} timeout The minimum length of time, in ms, for which to
 *     show the tool-bar. If the hide_() method is called within this time,
 *     it will not take effect until the timeout expires.
 * @return {void} Nothing.
 * @private
 */
remoting.Toolbar.prototype.showForAtLeast_ = function(timeout) {
  if (this.visible) {
    return;
  }
  addClass(this.toolbar_, remoting.Toolbar.VISIBLE_CLASS_);
  this.visible = true;
  if (this.timerId) {
    window.clearTimeout(this.timerId);
    this.timerId = null;
  }
  /** @type {remoting.Toolbar} */
  var that = this;
  var checkVisibility = function() { that.checkVisibility_(); };
  this.timerId = window.setTimeout(checkVisibility, timeout);
};

/**
 * Hide the tool-bar if it is visible. If there is a visibility timer running,
 * the tool-bar will not be hidden until it expires.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.Toolbar.prototype.hide_ = function() {
  if (!this.visible) {
    return;
  }
  this.visible = false;
  if (!this.timerId) {
    this.checkVisibility_();
  }
};

/**
 * Hide the tool-bar if it is visible and should not be.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.Toolbar.prototype.checkVisibility_ = function() {
  this.timerId = null;
  if (!this.visible) {
    removeClass(this.toolbar_, remoting.Toolbar.VISIBLE_CLASS_);
  }
};

/** @type {remoting.Toolbar} */
remoting.toolbar = null;

/** @private */
remoting.Toolbar.VISIBLE_CLASS_ = 'toolbar-visible';
