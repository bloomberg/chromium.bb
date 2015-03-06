// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function(){

'use strict';

/** @type {string} */
var NEW_WINDOW_MENU_ID_ = 'new-window';

/**
 * A class that handles application activation.
 *
 * @param {base.Ipc} ipc
 * @param {remoting.V2AppLauncher} appLauncher
 * @constructor
 */
remoting.ActivationHandler = function (ipc, appLauncher) {
  /** @private {remoting.V2AppLauncher} */
  this.appLauncher_ = appLauncher;

  chrome.contextMenus.create({
     id: NEW_WINDOW_MENU_ID_,
     contexts: ['launcher'],
     title: chrome.i18n.getMessage(/*i18n-content*/'NEW_WINDOW')
  });

  chrome.contextMenus.onClicked.addListener(this.onContextMenu_.bind(this));
  chrome.app.runtime.onLaunched.addListener(this.onLaunched_.bind(this));
  ipc.register(remoting.ActivationHandler.Ipc.RELAUNCH,
               appLauncher.restart.bind(appLauncher));
};

/** @enum {string} */
remoting.ActivationHandler.Ipc = {
  RELAUNCH: 'remoting.ActivationHandler.restart'
};

/**
 * @param {OnClickData} info
 * @private
 */
remoting.ActivationHandler.prototype.onContextMenu_ = function(info) {
  if (info.menuItemId == NEW_WINDOW_MENU_ID_) {
    this.appLauncher_.launch();
  }
};

/**
 * Called when the App is activated (e.g. from the Chrome App Launcher).  It
 * creates a new window if there are no existing ones.  Otherwise, it will put
 * focus on the last window created.
 *
 * @private
 */
remoting.ActivationHandler.prototype.onLaunched_ = function() {
  var windows = chrome.app.window.getAll();
  if (windows.length >= 1) {
    windows[windows.length - 1].focus();
  } else {
    this.appLauncher_.launch();
  }
};

})();
