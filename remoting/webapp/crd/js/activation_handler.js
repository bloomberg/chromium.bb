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
 * @extends {base.EventSourceImpl}
 * @implements {base.Disposable}
 * @constructor
 */
remoting.ActivationHandler = function (ipc, appLauncher) {
  base.inherits(this, base.EventSourceImpl);
  this.defineEvents(base.values(remoting.ActivationHandler.Events));

  /** @private {remoting.V2AppLauncher} */
  this.appLauncher_ = appLauncher;

  chrome.contextMenus.create({
     id: NEW_WINDOW_MENU_ID_,
     contexts: ['launcher'],
     title: chrome.i18n.getMessage(/*i18n-content*/'NEW_WINDOW')
  });

  this.disposables_ = new base.Disposables(
      new base.ChromeEventHook(chrome.contextMenus.onClicked,
                               this.onContextMenu_.bind(this)),
      new base.ChromeEventHook(chrome.app.runtime.onLaunched,
                               this.onLaunched_.bind(this)));
  ipc.register(remoting.ActivationHandler.Ipc.RELAUNCH,
               appLauncher.restart.bind(appLauncher));
};

remoting.ActivationHandler.prototype.dispose = function() {
  base.dispose(this.disposables_);
  this.disposables_ = null;
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
    this.onLaunched_();
  }
};

/**
 * Create a new window when the App is launched.
 *
 * @private
 */
remoting.ActivationHandler.prototype.onLaunched_ = function() {
  var that = this;
  this.appLauncher_.launch().then(function(/** string */ id) {
    var appWindow = chrome.app.window.get(id);
    that.disposables_.add(new base.ChromeEventHook(
        appWindow.onClosed, that.onWindowClosed_.bind(that, id)));
  });
};

/** @param {string} id The id of the window that is closed */
remoting.ActivationHandler.prototype.onWindowClosed_ = function(id) {
  this.raiseEvent(remoting.ActivationHandler.Events.windowClosed, id);
};

})();

/** @enum {string} */
remoting.ActivationHandler.Events = {
  windowClosed: 'windowClosed'
};
