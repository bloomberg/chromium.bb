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
  /** @private {remoting.ClientSession} */
  this.session_ = null;
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
  remoting.setMode(remoting.AppMode.IN_SESSION);
  if (!base.isAppsV2()) {
    remoting.toolbar.center();
    remoting.toolbar.preview();
  }

  this.session_ = connectionInfo.session();

  this.connectedView_ = new remoting.DesktopConnectedView(
      document.getElementById('client-container'), connectionInfo);

  // By default, under ChromeOS, remap the right Control key to the right
  // Win / Cmd key.
  if (remoting.platformIsChromeOS()) {
    connectionInfo.plugin().setRemapKeys('0x0700e4>0x0700e7');
  }

  if (connectionInfo.plugin().hasCapability(
          remoting.ClientSession.Capability.VIDEO_RECORDER)) {
    var recorder = new remoting.VideoFrameRecorder();
    connectionInfo.plugin().extensions().register(recorder);
    this.connectedView_.setVideoFrameRecorder(recorder);
  }

  this.parentActivity_.onConnected(connectionInfo);
};

remoting.DesktopRemotingActivity.prototype.onDisconnected = function() {
  this.parentActivity_.onDisconnected();
  this.dispose();
};

/**
 * @param {!remoting.Error} error
 */
remoting.DesktopRemotingActivity.prototype.onConnectionFailed =
    function(error) {
  this.parentActivity_.onConnectionFailed(error);
};

/**
 * @param {!remoting.Error} error The error to be localized and displayed.
 */
remoting.DesktopRemotingActivity.prototype.onError = function(error) {
  console.error('Connection failed: ' + error.toString());

  if (error.hasTag(remoting.Error.Tag.AUTHENTICATION_FAILED)) {
    remoting.setMode(remoting.AppMode.HOME);
    remoting.handleAuthFailureAndRelaunch();
    return;
  }

  this.parentActivity_.onError(error);

  this.dispose();
};

remoting.DesktopRemotingActivity.prototype.dispose = function() {
  base.dispose(this.connectedView_);
  this.connectedView_ = null;
  this.session_ = null;
};

/** @return {remoting.DesktopConnectedView} */
remoting.DesktopRemotingActivity.prototype.getConnectedView = function() {
  return this.connectedView_;
};

})();
