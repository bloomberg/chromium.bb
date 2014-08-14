// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to communicate with the background scripts via chrome runtime
 * messages to
 * 1. Forward session state notifications
 * 2. Closes the window when the session terminates
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {string} senderId id of the current tab or window.
 */
remoting.HangoutSession = function(senderId) {
  /**
   * @private
   * @type {chrome.runtime.Port}
   */
  this.port_ = null;

  /**
   * @private
   * @type {string}
   */
  this.senderId_ = senderId;
};

remoting.HangoutSession.prototype.init = function() {
  var portName = 'it2me.helper.webapp@' + this.senderId_;
  this.port_ = chrome.runtime.connect({name: portName});

  remoting.hangoutSessionEvents.addEventListener(
      remoting.hangoutSessionEvents.sessionStateChanged,
      this.onSessionStateChanged_.bind(this));
};

/**
 * @param {remoting.ClientSession.State} state
 */
remoting.HangoutSession.prototype.onSessionStateChanged_ = function(state) {
  var State = remoting.ClientSession.State;
  try {
    this.port_.postMessage({method: 'sessionStateChanged', state: state});
  } catch (e) {
    // postMessage will throw an exception if the port is disconnected.
    // We can safely ignore this exception.
    var error = /** @type {Error} */ e;
    console.error(error);
  } finally {
    if (state === State.FAILED || state === State.CLOSED) {
      // close the current window
      if (remoting.isAppsV2) {
        chrome.app.window.current().close();
      } else {
        window.close();
      }
    }
  }
};


/**
 * remoting.clientSession does not exist until the session is connected.
 * hangoutSessionEvents serves as a global event source to plumb session
 * state changes until we cleanup clientSession and sessionConnector.
 * @type {base.EventSource}
 */
remoting.hangoutSessionEvents = new base.EventSource();

/** @type {string} */
remoting.hangoutSessionEvents.sessionStateChanged = "sessionStateChanged";

remoting.hangoutSessionEvents.defineEvents(
    [remoting.hangoutSessionEvents.sessionStateChanged]);