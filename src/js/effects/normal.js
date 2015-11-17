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
camera.effects.Normal = function() {
  camera.Effect.call(this);
  Object.freeze(this);
};

camera.effects.Normal.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Normal.prototype.getTitle = function() {
  return chrome.i18n.getMessage('normalEffect');
};

