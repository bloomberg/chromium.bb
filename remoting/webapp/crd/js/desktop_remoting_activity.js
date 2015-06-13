// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * This class provides common functionality to Me2MeActivity and It2MeActivity,
 * e.g. constructing the relevant UX, and delegate the custom handling of the
 * session state changes to the |parentActivity|.
 *
 * @param {remoting.Activity} parentActivity
 * @implements {remoting.ClientSession.EventHandler}
 * @implements {base.Disposable}
 * @constructor
 */
remoting.DesktopRemotingActivity = function(parentActivity) {
  /** @private */
  this.parentActivity_ = parentActivity;
  /** @private {remoting.DesktopConnectedView} */
  this.connectedView_ = null;

  /** @private */
  this.sessionFactory_ = new remoting.ClientSessionFactory(
      document.querySelector('#client-container .client-plugin-container'),
      [/* No special capabilities required. */]);

  /** @private {remoting.ClientSession} */
  this.session_ = null;
  /** @private {remoting.ConnectingDialog} */
  this.connectingDialog_ =
      new remoting.ConnectingDialog(parentActivity.stop.bind(parentActivity));
};

/**
 * Initiates a connection.
 *
 * @param {remoting.Host} host the Host to connect to.
 * @param {remoting.CredentialsProvider} credentialsProvider
 * @param {boolean=} opt_suppressOfflineError
 * @return {void} Nothing.
 */
remoting.DesktopRemotingActivity.prototype.start =
    function(host, credentialsProvider, opt_suppressOfflineError) {
  var that = this;
  this.sessionFactory_.createSession(this).then(
    function(/** remoting.ClientSession */ session) {
      that.session_ = session;
      session.logHostOfflineErrors(!opt_suppressOfflineError);
      session.getLogger().setHostVersion(host.hostVersion);

      var mode = remoting.ServerLogEntry.VALUE_MODE_UNKNOWN;
      if (that.parentActivity_ instanceof remoting.It2MeActivity) {
          mode = remoting.ServerLogEntry.VALUE_MODE_IT2ME;
      } else if (that.parentActivity_ instanceof remoting.Me2MeActivity) {
          mode = remoting.ServerLogEntry.VALUE_MODE_ME2ME;
      }
      session.getLogger().setLogEntryMode(mode);

      session.connect(host, credentialsProvider);
  }).catch(remoting.Error.handler(
    function(/** !remoting.Error */ error) {
      that.parentActivity_.onConnectionFailed(error);
  }));
};

remoting.DesktopRemotingActivity.prototype.stop = function() {
  if (this.session_) {
    this.session_.disconnect(remoting.Error.none());
    console.log('Disconnected.');
  }
};

/**
 * @param {remoting.ConnectionInfo} connectionInfo
 */
remoting.DesktopRemotingActivity.prototype.onConnected =
    function(connectionInfo) {
  this.connectingDialog_.hide();
  remoting.setMode(remoting.AppMode.IN_SESSION);
  if (!base.isAppsV2()) {
    remoting.toolbar.center();
    remoting.toolbar.preview();
  }

  this.connectedView_ = new remoting.DesktopConnectedView(
      document.getElementById('client-container'), connectionInfo);

  // Apply the default or previously-specified keyboard remapping.
  var remapping = connectionInfo.host().options.remapKeys;
  if (base.isEmptyObject(remapping) && remoting.platformIsChromeOS()) {
    // Under ChromeOS, remap the right Control key to the right Win/Cmd key.
    remapping = {0x0700e4: 0x0700e7};
  }
  connectionInfo.plugin().setRemapKeys(remapping);

  if (connectionInfo.plugin().hasCapability(
          remoting.ClientSession.Capability.VIDEO_RECORDER)) {
    var recorder = new remoting.VideoFrameRecorder();
    connectionInfo.plugin().extensions().register(recorder);
    this.connectedView_.setVideoFrameRecorder(recorder);
  }

  this.parentActivity_.onConnected(connectionInfo);
};

remoting.DesktopRemotingActivity.prototype.onDisconnected = function(reason) {
  if (this.handleError_(reason)) {
    return;
  }
  this.parentActivity_.onDisconnected(reason);
};

/**
 * @param {!remoting.Error} error
 */
remoting.DesktopRemotingActivity.prototype.onConnectionFailed =
    function(error) {
  if (this.handleError_(error)) {
    return;
  }
  this.parentActivity_.onConnectionFailed(error);
};

/**
 * @param {!remoting.Error} error The error to be localized and displayed.
 * @return {boolean} returns true if the error is handled.
 * @private
 */
remoting.DesktopRemotingActivity.prototype.handleError_ = function(error) {
  if (error.hasTag(remoting.Error.Tag.AUTHENTICATION_FAILED)) {
    remoting.setMode(remoting.AppMode.HOME);
    remoting.handleAuthFailureAndRelaunch();
    return true;
  }
  return false;
};

remoting.DesktopRemotingActivity.prototype.dispose = function() {
  base.dispose(this.connectedView_);
  this.connectedView_ = null;
  base.dispose(this.session_);
  this.session_ = null;
  this.connectingDialog_.hide();
};

/** @return {remoting.DesktopConnectedView} */
remoting.DesktopRemotingActivity.prototype.getConnectedView = function() {
  return this.connectedView_;
};

/**
 * @return {remoting.ClientSession}.
 */
remoting.DesktopRemotingActivity.prototype.getSession = function() {
  return this.session_;
};

/** @return {remoting.ConnectingDialog} */
remoting.DesktopRemotingActivity.prototype.getConnectingDialog = function() {
  return this.connectingDialog_;
};

})();
