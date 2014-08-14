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
  this.onMessageExternalRef_ = this.onMessageExternal_.bind(this);
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
  chrome.runtime.onMessageExternal.addListener(this.onMessageExternalRef_);
  chrome.runtime.onConnectExternal.addListener(this.onConnectExternalRef_);
};

remoting.It2MeService.prototype.dispose = function() {
  chrome.runtime.onConnect.removeListener(this.onWebappConnectRef_);
  chrome.runtime.onMessageExternal.removeListener(
      this.onMessageExternalRef_);
  chrome.runtime.onConnectExternal.removeListener(
      this.onConnectExternalRef_);
};

/**
 * This function is called when a runtime message is received from an external
 * web page (hangout) or extension.
 *
 * @param {{method:string, data:Object.<string,*>}} message
 * @param {chrome.runtime.MessageSender} sender
 * @param {function(*):void} sendResponse
 * @private
 */
remoting.It2MeService.prototype.onMessageExternal_ =
    function(message, sender, sendResponse) {
  try {
    var method = message.method;
    if (method == 'hello') {
      // The hello message is used by hangouts to detect whether the app is
      // installed and what features are supported.
      sendResponse({
        method: 'helloResponse',
        supportedFeatures: ['it2me']
      });
      return true;
    }
    throw new Error('Unknown method: ' + method);
  } catch (e) {
    var error = /** @type {Error} */ e;
    console.error(error);
    sendResponse({
      method: message.method + 'Response',
      error: error.message
    });
  }
  return false;
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
  }

  console.log('Incoming helper connection from Hangouts');
  var helper = new remoting.It2MeHelperChannel(
      this.appLauncher_, port, this.onHelperChannelDisconnected.bind(this));
  helper.init();
  this.helpers_.push(helper);
};
