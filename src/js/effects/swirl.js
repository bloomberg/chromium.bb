// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for effects.
 */
camera.effects = camera.effects || {};

/**
 * @constructor
 * @extends {camera.Effect}
 */
camera.effects.Swirl = function() {
  camera.Effect.call(this);
  Object.freeze(this);
};

/**
 * Returns true if the effect is available on the platform.
 *
 * @param {Array.<string>} webglExtensions Supported WebGL extensions on the
 *     platform.
 * @return {boolean} True if available, false otherwise.
 */
camera.effects.Swirl.isAvailable = function(webglExtensions) {
  return true;
};

camera.effects.Swirl.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Swirl.prototype.filterFrame = function(canvas, faces) {
  var face = faces[0];
  var x = canvas.width * (face.x + (face.width / 2));
  var y = canvas.height * face.y;
  var radius = Math.sqrt(Math.pow(face.width * canvas.width, 2) +
                         Math.pow(face.height * canvas.height, 2));
  canvas.swirl(x, y, radius, 1.5 * Math.sin(Date.now() / 300));
};

/**
 * @override
 */
camera.effects.Swirl.prototype.getTitle = function() {
  return chrome.i18n.getMessage('swirlEffect');
};

/**
 * @override
 */
camera.effects.Swirl.prototype.usesHeadTracker = function() {
  return true;
};

