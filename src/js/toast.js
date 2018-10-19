// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for toast.
 */
camera.toast = camera.toast || {};

/**
 * Shows a toast message.
 * @param {string} message Message to be shown.
 */
camera.toast.show = function(message) {
  var element = document.querySelector('#toast');
  camera.util.animateCancel(element); // Cancel the showing toast if any.
  element.textContent = chrome.i18n.getMessage(message) || message;
  camera.util.animateOnce(element);
};
