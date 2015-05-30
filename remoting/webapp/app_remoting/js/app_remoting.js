// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This class implements the functionality that is specific to application
 * remoting ("AppRemoting" or AR).
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Array<string>} appCapabilities Array of application capabilities.
 * @constructor
 * @implements {remoting.ApplicationInterface}
 * @extends {remoting.Application}
 */
remoting.AppRemoting = function(appCapabilities) {
  base.inherits(this, remoting.Application);

  /** @private {remoting.Activity} */
  this.activity_ = null;

  /** @private */
  this.appCapabilities_ = appCapabilities;
};

/**
 * @return {string} Application product name to be used in UI.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.getApplicationName = function() {
  var manifest = chrome.runtime.getManifest();
  return manifest.name;
};

remoting.AppRemoting.prototype.getActivity = function() {
  return this.activity_;
};

/**
 * @param {!remoting.Error} error The failure reason.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.signInFailed_ = function(error) {
  remoting.MessageWindow.showErrorMessage(
      this.getApplicationName(),
      chrome.i18n.getMessage(error.getTag()));
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.initApplication_ = function() {
  var windowShape = new remoting.WindowShape();
  windowShape.updateClientWindowShape();

  this.activity_ = new remoting.AppRemotingActivity(this.appCapabilities_, this,
                                                    windowShape);
};

/**
 * @param {string} token An OAuth access token.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.startApplication_ = function(token) {
  this.activity_.start();
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.exitApplication_ = function() {
  this.activity_.dispose();
  this.closeMainWindow_();
};
