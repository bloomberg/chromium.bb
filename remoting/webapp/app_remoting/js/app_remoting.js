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
 * Parameters for the remoting.AppRemoting constructor.
 *
 * appId: The application ID. If this is not specified than the app id will
 *     be extracted from the app's manifest.
 *
 * appCapabilites: Array of application capabilites.
 *
 * licenseManager: Licence manager for this application.
 *
 * @typedef {{
 *   appId: (string|undefined),
 *   appCapabilities: (Array<string>|undefined),
 *   licenseManager: (remoting.LicenseManager|undefined)
 * }}
 */
remoting.AppRemotingParams;

/**
 * @param {remoting.AppRemotingParams} args
 * @constructor
 * @implements {remoting.ApplicationInterface}
 * @extends {remoting.Application}
 */
remoting.AppRemoting = function(args) {
  base.inherits(this, remoting.Application);
  remoting.app = this;

  // Save recent errors for inclusion in user feedback.
  remoting.ConsoleWrapper.getInstance().activate(
      5,
      remoting.ConsoleWrapper.LogType.ERROR,
      remoting.ConsoleWrapper.LogType.ASSERT);

  /** @private {remoting.Activity} */
  this.activity_ = null;

  /** @private {string} */
  this.appId_ = (args.appId) ? args.appId : chrome.runtime.id;

  /** @private */
  this.licenseManager_ = (args.licenseManager) ?
                             args.licenseManager :
                             new remoting.GaiaLicenseManager();

  /** @private */
  this.appCapabilities_ = (args.appCapabilities) ? args.appCapabilities : [];

  // This prefix must be added to message window paths so that the HTML
  // files can be found in the shared module.
  // TODO(garykac) Add support for dev/prod shared modules.
  remoting.MessageWindow.htmlFilePrefix =
      "_modules/koejkfhmphamcgafjmkellhnekdkopod/";
};

/**
 * @return {string} Application Id.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.getApplicationId = function() {
  return this.appId_;
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
  remoting.messageWindowManager = new remoting.MessageWindowManager(
      /** @type {base.WindowMessageDispatcher} */
      (this.windowMessageDispatcher_));
};

/**
 * @param {string} token An OAuth access token.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.startApplication_ = function(token) {
  var windowShape = new remoting.WindowShape();
  windowShape.updateClientWindowShape();
  var that = this;

  this.licenseManager_.getSubscriptionToken(token).then(
      function(/** string*/ subscriptionToken) {
    that.activity_ = new remoting.AppRemotingActivity(
        that.appCapabilities_, that, windowShape, subscriptionToken,
        /** @type {base.WindowMessageDispatcher} */
        (that.windowMessageDispatcher_));
    that.activity_.start();
  });
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.exitApplication_ = function() {
  if (this.activity_) {
    this.activity_.stop();
    this.activity_.dispose();
    this.activity_ = null;
  }
  this.closeMainWindow_();
};
