// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.WindowShape.ClientUI}
 * @param {HTMLElement} element The dialog DOM element.
 */
remoting.AuthDialog = function(element) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;
};

/**
 * @param {Array.<{left: number, top: number, width: number, height: number}>}
 *     rects List of rectangles.
 */
remoting.AuthDialog.prototype.addToRegion = function(rects) {
  var rect = /** @type {ClientRect} */(this.element_.getBoundingClientRect());
  rects.push({left: rect.left,
              top: rect.top,
              width: rect.width,
              height: rect.height});
}

/** @type {remoting.AuthDialog} */
remoting.authDialog = null;
