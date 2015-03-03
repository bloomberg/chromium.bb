// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function(){

'use strict';

var ENABLE_HANGOUT_REMOTE_ASSISTANCE = false;

/**
 * @constructor
 */
var BackgroundPage = function() {
  /** @private {remoting.AppLauncher} */
  this.appLauncher_ = null;
  /** @private {remoting.ActivationHandler} */
  this.activationHandler_ = null;
  /** @private {remoting.It2MeService} */
  this.it2meService_ = null;
  /** @private {base.Disposables} */
  this.disposables_ = null;

  this.preInit_();
  this.onResumed_();

  chrome.runtime.onSuspendCanceled.addListener(this.onResumed_.bind(this));
  chrome.runtime.onSuspend.addListener(this.onSuspended_.bind(this));
};

/**
 * Initialize members and globals that are valid throughout the entire lifetime
 * of the background page.
 *
 * @private
 */
BackgroundPage.prototype.preInit_ = function() {
  remoting.settings = new remoting.Settings();
  if (base.isAppsV2()) {
    remoting.identity = new remoting.Identity();
  } else {
    remoting.oauth2 = new remoting.OAuth2();
    var oauth2 = /** @type {*} */ (remoting.oauth2);
    remoting.identity = /** @type {remoting.Identity} */ (oauth2);
  }

  if (base.isAppsV2()) {
    this.appLauncher_ = new remoting.V2AppLauncher();
    this.activationHandler_ = new remoting.ActivationHandler(
        base.Ipc.getInstance(), this.appLauncher_);
  } else {
    this.appLauncher_ = new remoting.V1AppLauncher();
  }
};

/** @private */
BackgroundPage.prototype.onResumed_ = function() {
  if (ENABLE_HANGOUT_REMOTE_ASSISTANCE) {
    this.it2meService_ = new remoting.It2MeService(this.appLauncher_);
    this.it2meService_.init();
    this.disposables_ = new base.Disposables(this.it2meService_);
  }
};

/** @private */
BackgroundPage.prototype.onSuspended_ = function() {
  this.it2meService_ = null;
  base.dispose(this.disposables_);
  this.disposables_ = null;
};

window.addEventListener('load', function() {
  remoting.backgroundPage = new BackgroundPage();
}, false);

}());
