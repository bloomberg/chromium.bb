// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @see https://developer.chrome.com/apps/app_window#type-AppWindow
 *
 * TODO(yawano): Remove this since chrome.app.window.AppWindow is already
 *     defined.
 */
function AppWindow() {}

/** @type {Window} */
AppWindow.prototype.contentWindow;
