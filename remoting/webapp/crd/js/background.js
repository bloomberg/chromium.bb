// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function(){

/**
 * A class that handles application activation.
 *
 * @param {remoting.AppLauncher} appLauncher
 * @constructor
 */
function ActivationHandler(appLauncher) {
  /**
   * @type {remoting.AppLauncher}
   * @private
   */
  this.appLauncher_ = appLauncher;

  chrome.contextMenus.create({
     id: ActivationHandler.NEW_WINDOW_MENU_ID_,
     contexts: ['launcher'],
     title: chrome.i18n.getMessage(/*i18n-content*/'NEW_WINDOW')
  });

  chrome.contextMenus.onClicked.addListener(this.onContextMenu_.bind(this));
  chrome.app.runtime.onLaunched.addListener(this.onLaunched_.bind(this));
}

/** @type {string} */
ActivationHandler.NEW_WINDOW_MENU_ID_ = 'new-window';

/**
 * @param {OnClickData} info
 * @private
 */
ActivationHandler.prototype.onContextMenu_ = function(info) {
  if (info.menuItemId == ActivationHandler.NEW_WINDOW_MENU_ID_) {
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
ActivationHandler.prototype.onLaunched_ = function() {
  var windows = chrome.app.window.getAll();
  if (windows.length >= 1) {
    windows[windows.length - 1].focus();
  } else {
    this.appLauncher_.launch();
  }
};

/**
 * The background service is responsible for listening to incoming connection
 * requests from Hangouts and the webapp.
 *
 * @param {remoting.AppLauncher} appLauncher
 */
function initializeBackgroundService(appLauncher) {
  function initializeIt2MeService() {
    /** @type {remoting.It2MeService} */
    remoting.it2meService = new remoting.It2MeService(appLauncher);
    remoting.it2meService.init();
  }

  chrome.runtime.onSuspend.addListener(function() {
    base.debug.assert(remoting.it2meService != null);
    remoting.it2meService.dispose();
    remoting.it2meService = null;
  });

  remoting.settings = new remoting.Settings();

  chrome.runtime.onSuspendCanceled.addListener(initializeIt2MeService);
  initializeIt2MeService();
}

function main() {
  if (base.isAppsV2()) {
    new ActivationHandler(new remoting.V2AppLauncher());
  }
}

remoting.enableHangoutsRemoteAssistance = function() {
  /** @type {remoting.AppLauncher} */
  var appLauncher = base.isAppsV2() ? new remoting.V1AppLauncher():
                                      new remoting.V2AppLauncher();
  initializeBackgroundService(appLauncher);
};

window.addEventListener('load', main, false);

}());
