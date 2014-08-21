// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * It2MeService listens to incoming connections requests from Hangouts
 * and the webapp and creates a It2MeHelperChannel between them.
 * It supports multiple helper sessions, but only a single helpee.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.AppLauncher} appLauncher
 * @constructor
 */
remoting.It2MeService = function(appLauncher) {
  /**
   * @type {remoting.AppLauncher}
   * @private
   */
  this.appLauncher_ = appLauncher;

  /**
   * @type {Array.<remoting.It2MeHelperChannel>}
   * @private
   */
  this.helpers_ = [];

  /** @private */
  this.helpee_ = null;

  this.onWebappConnectRef_ = this.onWebappConnect_.bind(this);
  this.onConnectExternalRef_ = this.onConnectExternal_.bind(this);
};

/** @enum {string} */
remoting.It2MeService.ConnectionTypes = {
  HELPER_HANGOUT: 'it2me.helper.hangout',
  HELPEE_HANGOUT: 'it2me.helpee.hangout',
  HELPER_WEBAPP: 'it2me.helper.webapp'
};

/**
 * Starts listening to external connection from Hangouts and the webapp.
 */
remoting.It2MeService.prototype.init = function() {
  chrome.runtime.onConnect.addListener(this.onWebappConnectRef_);
  chrome.runtime.onConnectExternal.addListener(this.onConnectExternalRef_);
};

remoting.It2MeService.prototype.dispose = function() {
  chrome.runtime.onConnect.removeListener(this.onWebappConnectRef_);
  chrome.runtime.onConnectExternal.removeListener(
      this.onConnectExternalRef_);
};

/**
 * This function is called when Hangouts connects via chrome.runtime.connect.
 * Only web pages that are white-listed in the manifest are allowed to connect.
 *
 * @param {chrome.runtime.Port} port
 * @private
 */
remoting.It2MeService.prototype.onConnectExternal_ = function(port) {
  var ConnectionTypes = remoting.It2MeService.ConnectionTypes;
  try {
    switch (port.name) {
      case ConnectionTypes.HELPER_HANGOUT:
        this.handleExternalHelperConnection_(port);
        return true;
      case ConnectionTypes.HELPEE_HANGOUT:
        this.handleExternalHelpeeConnection_(port);
        return true;
      default:
        throw new Error('Unsupported port - ' + port.name);
    }
  } catch (e) {
    var error = /**@type {Error} */ e;
    console.error(error);
    port.disconnect();
  }
  return false;
};

/**
 * @param {chrome.runtime.Port} port
 * @private
 */
remoting.It2MeService.prototype.onWebappConnect_ = function(port) {
  try {
    console.log('Incoming helper connection from webapp.');

    // The senderId (tabId or windowId) of the webapp is embedded in the port
    // name with the format port_name@senderId.
    var parts = port.name.split('@');
    var portName = parts[0];
    var senderId = parts[1];
    var ConnectionTypes = remoting.It2MeService.ConnectionTypes;
    if (portName === ConnectionTypes.HELPER_WEBAPP && senderId !== undefined) {
      for (var i = 0; i < this.helpers_.length; i++) {
        var helper = this.helpers_[i];
        if (helper.instanceId() === senderId) {
          helper.onWebappConnect(port, senderId);
          return;
        }
      }
    }
    throw new Error('No matching hangout connection found for ' + port.name);
  } catch (e) {
    var error = /** @type {Error} */ e;
    console.error(error);
    port.disconnect();
  }
};

/**
 * @param {remoting.It2MeHelperChannel} helper
 */
remoting.It2MeService.prototype.onHelperChannelDisconnected = function(helper) {
  for (var i = 0; i < this.helpers_.length; i++) {
    if (helper === this.helpers_[i]) {
      this.helpers_.splice(i, 1);
    }
  }
};

remoting.It2MeService.prototype.onHelpeeChannelDisconnected = function() {
  base.debug.assert(this.helpee_ !== null);
  this.helpee_ = null;
};

/**
 * @param {chrome.runtime.Port} port
 * @private
 */
remoting.It2MeService.prototype.handleExternalHelperConnection_ =
    function(port) {
  if (this.helpee_) {
    console.error(
        'Cannot start a helper session while a helpee session is in process.');
    port.disconnect();
    return;
  }

  console.log('Incoming helper connection from Hangouts');
  var helper = new remoting.It2MeHelperChannel(
      this.appLauncher_, port, this.onHelperChannelDisconnected.bind(this));
  helper.init();
  this.helpers_.push(helper);
};

/**
 * @param {chrome.runtime.Port} hangoutPort Represents a connection to Hangouts.
 * @private
 */
remoting.It2MeService.prototype.handleExternalHelpeeConnection_ =
    function(hangoutPort) {
  if (this.helpee_) {
    console.error('An existing helpee session is in process.');
    hangoutPort.disconnect();
    return;
  }

  console.log('Incoming helpee connection from Hangouts');
  this.helpee_ = new remoting.It2MeHelpeeChannel(
      hangoutPort,
      new remoting.It2MeHostFacade(),
      new remoting.HostInstaller(),
      this.onHelpeeChannelDisconnected.bind(this));
  this.helpee_.init();
};