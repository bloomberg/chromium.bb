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

  /** @private */
  this.ipc_ = ipc;

  /** @private {remoting.V2AppLauncher} */
  this.appLauncher_ = appLauncher;

  /** @private {Map<string, base.Disposable>} */
  this.windowClosedHooks_ = new Map();

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
  ipc.register(remoting.ActivationHandler.Ipc.RESTART,
               this.onRestart_.bind(this));
  ipc.register(remoting.ActivationHandler.Ipc.LAUNCH,
               this.onLaunched_.bind(this));

};

remoting.ActivationHandler.prototype.dispose = function() {
  this.windowClosedHooks_.forEach(function(/** base.Disposable */ eventHook) {
    base.dispose(eventHook);
  });
  this.windowClosedHooks_.clear();

  base.dispose(this.disposables_);
  this.disposables_ = null;
  this.ipc_.unregister(remoting.ActivationHandler.Ipc.LAUNCH);
  this.ipc_.unregister(remoting.ActivationHandler.Ipc.RESTART);
};

/** @enum {string} */
remoting.ActivationHandler.Ipc = {
  LAUNCH: 'remoting.ActivationHandler.launch',
  RESTART: 'remoting.ActivationHandler.restart'
};

/**
 * @param {OnClickData} info
 * @private
 */
remoting.ActivationHandler.prototype.onContextMenu_ = function(info) {
  if (info.menuItemId == NEW_WINDOW_MENU_ID_) {
    this.createWindow_();
  }
};

/**
 * Restart the window with |id|.
 * @param {string} id
 *
 * @private
 */
remoting.ActivationHandler.prototype.onRestart_ = function(id) {
  this.appLauncher_.restart(id).then(this.registerCloseListener_.bind(this));
};

/**
 * @private
 */
remoting.ActivationHandler.prototype.onLaunched_ = function() {
  this.createWindow_();
};

/**
 * Create a new app window and register for the closed event.
 *
 * @private
 */
remoting.ActivationHandler.prototype.createWindow_ = function() {
  this.appLauncher_.launch().then(this.registerCloseListener_.bind(this));
};

/**
 * @param {string} windowId
 *
 * @private
 */
remoting.ActivationHandler.prototype.registerCloseListener_ = function(
    windowId) {
  var appWindow = chrome.app.window.get(windowId);
  console.assert(!this.windowClosedHooks_.has(windowId),
                'Duplicate close listener attached to window : ' + windowId);
  this.windowClosedHooks_.set(windowId, new base.ChromeEventHook(
      appWindow.onClosed, this.onWindowClosed_.bind(this, windowId)));
};

/**
 * @param {string} id The id of the window that is closed.
 * @private
 */
remoting.ActivationHandler.prototype.onWindowClosed_ = function(id) {
  // Unhook the event.
  var hook = /** @type {base.Disposable} */ (this.windowClosedHooks_.get(id));
  base.dispose(hook);
  this.windowClosedHooks_.delete(id);

  this.raiseEvent(remoting.ActivationHandler.Events.windowClosed, id);
};

})();

/** @enum {string} */
remoting.ActivationHandler.Events = {
  windowClosed: 'windowClosed'
};
