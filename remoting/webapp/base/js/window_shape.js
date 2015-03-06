// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling setting of the local app window shape to account for windows
 * on the remote desktop, as well as any client-side UI.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor */
remoting.WindowShape = function() {
  /** @private {Array<{left: number, top: number,
                       width: number, height: number}>} */
  this.desktopRects_ = [];

  /** @private {Array<remoting.WindowShape.ClientUI>} */
  this.clientUICallbacks_ = [];
};

/**
 * @return {boolean} True if setShape is available.
 */
remoting.WindowShape.isSupported = function() {
  return base.isAppsV2() &&
      typeof(chrome.app.window.current().setShape) != 'undefined';
}

/**
 * Add a client-side UI measurement callback.
 *
 * @param {remoting.WindowShape.ClientUI} callback
 */
remoting.WindowShape.prototype.addCallback = function(callback) {
  this.clientUICallbacks_.push(callback);
  this.updateClientWindowShape();
};

/**
 * Set the region associated with the remote desktop windows.
 *
 * @param {Array<{left: number, top: number, width: number, height: number}>}
 *     rects
 */
remoting.WindowShape.prototype.setDesktopRects = function(rects) {
  this.desktopRects_ = rects;
  this.updateClientWindowShape();
};

/**
 * Update the client window shape.
 */
remoting.WindowShape.prototype.updateClientWindowShape = function() {
  if (!remoting.WindowShape.isSupported()) {
    return;
  }

  var rects = this.desktopRects_.slice();
  for (var i = 0; i < this.clientUICallbacks_.length; ++i) {
    this.clientUICallbacks_[i].addToRegion(rects);
  }
  for (var i = 0; i < rects.length; ++i) {
    var rect = /** @type {ClientRect} */ (rects[i]);
    var left = Math.floor(rect.left);
    var right = Math.ceil(rect.left + rect.width);
    var top = Math.floor(rect.top);
    var bottom = Math.ceil(rect.top + rect.height);
    rects[i] = { left: left,
                 top: top,
                 width: right - left,
                 height: bottom - top };
  }
  chrome.app.window.current().setShape({rects: rects});
};


/**
 * @interface
 */
remoting.WindowShape.ClientUI = function () {
};

/**
 * Add the context menu's bounding rectangle to the specified region.
 *
 * @param {Array<{left: number, top: number, width: number, height: number}>}
 *     rects
 */
remoting.WindowShape.ClientUI.prototype.addToRegion = function(rects) {};


/** @type {remoting.WindowShape} */
remoting.windowShape = new remoting.WindowShape();
