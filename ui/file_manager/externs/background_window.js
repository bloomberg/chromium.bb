// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {Window}
 */
var BackgroundWindow = function() {};

/**
 * @type {FileBrowserBackground}
 */
BackgroundWindow.prototype.background;

/**
 * @param {Window} window
 */
BackgroundWindow.prototype.registerDialog = function(window) {};

/**
 * @param {Object=} opt_appState App state.
 * @param {number=} opt_id Window id.
 * @param {LaunchType=} opt_type Launch type. Default: ALWAYS_CREATE.
 * @param {function(string)=} opt_callback Completion callback with the App ID.
 */
BackgroundWindow.prototype.launchFileManager =
    function(opt_appState, opt_id, opt_type, opt_callback) {};