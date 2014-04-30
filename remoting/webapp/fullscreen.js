// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Controller interface for full-screen mode.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @interface */
remoting.Fullscreen = function() { };

/**
 * Enter or leave full-screen mode.
 *
 * @param {boolean} fullscreen True to enter full-screen mode; false to leave.
 * @param {function():void=} opt_onDone Optional completion callback.
 */
remoting.Fullscreen.prototype.activate = function(fullscreen, opt_onDone) { };

/**
 * @return {boolean} True if full-screen mode is active.
 */
remoting.Fullscreen.prototype.isActive = function() { };

/**
 * Toggle full-screen mode.
 */
remoting.Fullscreen.prototype.toggle = function() { };

/**
 * Add a listener for the full-screen-changed event.
 *
 * @param {function(boolean):void} callback
 */
remoting.Fullscreen.prototype.addListener = function(callback) { };

/**
 * Remove a listener for the full-screen-changed event.
 *
 * @param {function(boolean):void} callback
 */
remoting.Fullscreen.prototype.removeListener = function(callback) { };

/**
 * Enable or disable automatic synchronization of full-screen and maximized
 * states. This allows the application to enter full-screen mode whenever its
 * window is maximized, regardless of how the user initiates this (clicking
 * the maximize control, double-clicking the title bar or using the tray menu,
 * for example). If the window is already maximized when this synchronization
 * is enabled, it is full-screened.
 *
 * This method is a no-op for apps v1.
 *
 * @param {boolean} sync True to enable synchronization; false to disable.
 */
remoting.Fullscreen.prototype.syncWithMaximize = function(sync) { };

/** @type {remoting.Fullscreen} */
remoting.fullscreen = null;
