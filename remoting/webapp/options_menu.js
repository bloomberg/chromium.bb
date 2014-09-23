// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling the in-session options menu (or menus in the case of apps v1).
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Element} sendCtrlAltDel
 * @param {Element} sendPrtScrn
 * @param {Element} resizeToClient
 * @param {Element} shrinkToFit
 * @param {Element} newConnection
 * @param {Element?} fullscreen
 * @param {Element?} startStopRecording
 * @constructor
 */
remoting.OptionsMenu = function(sendCtrlAltDel, sendPrtScrn,
                                resizeToClient, shrinkToFit,
                                newConnection, fullscreen,
                                startStopRecording) {
  this.sendCtrlAltDel_ = sendCtrlAltDel;
  this.sendPrtScrn_ = sendPrtScrn;
  this.resizeToClient_ = resizeToClient;
  this.shrinkToFit_ = shrinkToFit;
  this.newConnection_ = newConnection;
  this.fullscreen_ = fullscreen;
  this.startStopRecording_ = startStopRecording;
  /**
   * @type {remoting.ClientSession}
   * @private
   */
  this.clientSession_ = null;

  this.sendCtrlAltDel_.addEventListener(
      'click', this.onSendCtrlAltDel_.bind(this), false);
  this.sendPrtScrn_.addEventListener(
      'click', this.onSendPrtScrn_.bind(this), false);
  this.resizeToClient_.addEventListener(
      'click', this.onResizeToClient_.bind(this), false);
  this.shrinkToFit_.addEventListener(
      'click', this.onShrinkToFit_.bind(this), false);
  this.newConnection_.addEventListener(
      'click', this.onNewConnection_.bind(this), false);
  if (this.fullscreen_) {
    this.fullscreen_.addEventListener(
        'click', this.onFullscreen_.bind(this), false);
  }
  if (this.startStopRecording_) {
    this.startStopRecording_.addEventListener(
        'click', this.onStartStopRecording_.bind(this), false);
  }
};

/**
 * @param {remoting.ClientSession} clientSession The active session, or null if
 *     there is no connection.
 */
remoting.OptionsMenu.prototype.setClientSession = function(clientSession) {
  this.clientSession_ = clientSession;
};

remoting.OptionsMenu.prototype.onShow = function() {
  if (this.clientSession_) {
    this.resizeToClient_.hidden =
        this.clientSession_.getMode() == remoting.ClientSession.Mode.IT2ME;
    remoting.MenuButton.select(
        this.resizeToClient_, this.clientSession_.getResizeToClient());
    remoting.MenuButton.select(
        this.shrinkToFit_, this.clientSession_.getShrinkToFit());
    if (this.fullscreen_) {
      remoting.MenuButton.select(
          this.fullscreen_, remoting.fullscreen.isActive());
    }
    if (this.startStopRecording_) {
      this.startStopRecording_.hidden = !this.clientSession_.canRecordVideo();
      if (this.clientSession_.isRecordingVideo()) {
        l10n.localizeElementFromTag(this.startStopRecording_,
                                    /*i18n-content*/'STOP_RECORDING');
      } else {
        l10n.localizeElementFromTag(this.startStopRecording_,
                                    /*i18n-content*/'START_RECORDING');
      }
    }
  }
};

remoting.OptionsMenu.prototype.onSendCtrlAltDel_ = function() {
  if (this.clientSession_) {
    this.clientSession_.sendCtrlAltDel();
  }
};

remoting.OptionsMenu.prototype.onSendPrtScrn_ = function() {
  if (this.clientSession_) {
    this.clientSession_.sendPrintScreen();
  }
};

remoting.OptionsMenu.prototype.onResizeToClient_ = function() {
  if (this.clientSession_) {
    this.clientSession_.setScreenMode(this.clientSession_.getShrinkToFit(),
                                      !this.clientSession_.getResizeToClient());
  }
};

remoting.OptionsMenu.prototype.onShrinkToFit_ = function() {
  if (this.clientSession_) {
    this.clientSession_.setScreenMode(!this.clientSession_.getShrinkToFit(),
                                      this.clientSession_.getResizeToClient());
  }
};

remoting.OptionsMenu.prototype.onNewConnection_ = function() {
  chrome.app.window.create('main.html', {
    'width': 800,
    'height': 600,
    'frame': 'none'
  });
};

remoting.OptionsMenu.prototype.onFullscreen_ = function() {
  remoting.fullscreen.toggle();
};

remoting.OptionsMenu.prototype.onStartStopRecording_ = function() {
  if (this.clientSession_) {
    this.clientSession_.startStopRecording();
  }
}

/**
 * @type {remoting.OptionsMenu}
 */
remoting.optionsMenu = null;