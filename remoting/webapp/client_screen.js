// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions related to the 'client screen' for Chromoting.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @type {remoting.SessionConnector} The connector object, set when a connection
 *     is initiated.
 */
remoting.connector = null;

/**
 * @type {remoting.ClientSession} The client session object, set once the
 *     connector has invoked its onOk callback.
 */
remoting.clientSession = null;

/**
 * Initiate an IT2Me connection.
 */
remoting.connectIT2Me = function() {
  remoting.connector = new remoting.SessionConnector(
      document.getElementById('session-mode'),
      remoting.onConnected,
      showConnectError_);
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
  remoting.connector.connectIT2Me(accessCode);
};

/**
 * Update the remoting client layout in response to a resize event.
 *
 * @return {void} Nothing.
 */
remoting.onResize = function() {
  if (remoting.clientSession) {
    remoting.clientSession.onResize();
  }
};

/**
 * Handle changes in the visibility of the window, for example by pausing video.
 *
 * @return {void} Nothing.
 */
remoting.onVisibilityChanged = function() {
  if (remoting.clientSession) {
    remoting.clientSession.pauseVideo(document.webkitHidden);
  }
}

/**
 * Disconnect the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.disconnect = function() {
  if (!remoting.clientSession) {
    return;
  }
  if (remoting.clientSession.mode == remoting.ClientSession.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
  }
  remoting.clientSession.disconnect(true);
  remoting.clientSession = null;
  console.log('Disconnected.');
};

/**
 * Sends a Ctrl-Alt-Del sequence to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.sendCtrlAltDel = function() {
  if (remoting.clientSession) {
    console.log('Sending Ctrl-Alt-Del.');
    remoting.clientSession.sendCtrlAltDel();
  }
};

/**
 * Sends a Print Screen keypress to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.sendPrintScreen = function() {
  if (remoting.clientSession) {
    console.log('Sending Print Screen.');
    remoting.clientSession.sendPrintScreen();
  }
};

/**
 * Callback function called when the state of the client plugin changes. The
 * current state is available via the |state| member variable.
 *
 * @param {number} oldState The previous state of the plugin.
 * @param {number} newState The current state of the plugin.
 */
function onClientStateChange_(oldState, newState) {
  switch (newState) {
    case remoting.ClientSession.State.CLOSED:
      console.log('Connection closed by host');
      if (remoting.clientSession.mode == remoting.ClientSession.Mode.IT2ME) {
        remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
      } else {
        remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
      }
      break;

    case remoting.ClientSession.State.FAILED:
      var error = remoting.clientSession.getError();
      console.error('Client plugin reported connection failed: ' + error);
      if (error == null) {
        error = remoting.Error.UNEXPECTED;
      }
      showConnectError_(error);
      break;

    default:
      console.error('Unexpected client plugin state: ' + newState);
      // This should only happen if the web-app and client plugin get out of
      // sync, so MISSING_PLUGIN is a suitable error.
      showConnectError_(remoting.Error.MISSING_PLUGIN);
      break;
  }
  remoting.clientSession.disconnect(false);
  remoting.clientSession.removePlugin();
  remoting.clientSession = null;
}

/**
 * Show a client-side error message.
 *
 * @param {remoting.Error} errorTag The error to be localized and
 *     displayed.
 * @return {void} Nothing.
 */
function showConnectError_(errorTag) {
  console.error('Connection failed: ' + errorTag);
  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, /** @type {string} */ (errorTag));
  remoting.accessCode = '';
  var mode = remoting.clientSession ? remoting.clientSession.mode
                                    : remoting.connector.getConnectionMode();
  if (mode == remoting.ClientSession.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_ME2ME);
  }
}

/**
 * Set the text on the buttons shown under the error message so that they are
 * easy to understand in the case where a successful connection failed, as
 * opposed to the case where a connection never succeeded.
 */
function setConnectionInterruptedButtonsText_() {
  var button1 = document.getElementById('client-reconnect-button');
  l10n.localizeElementFromTag(button1, /*i18n-content*/'RECONNECT');
  button1.removeAttribute('autofocus');
  var button2 = document.getElementById('client-finished-me2me-button');
  l10n.localizeElementFromTag(button2, /*i18n-content*/'OK');
  button2.setAttribute('autofocus', 'autofocus');
}

/**
 * Timer callback to update the statistics panel.
 */
function updateStatistics_() {
  if (!remoting.clientSession ||
      remoting.clientSession.state != remoting.ClientSession.State.CONNECTED) {
    return;
  }
  var perfstats = remoting.clientSession.getPerfStats();
  remoting.stats.update(perfstats);
  remoting.clientSession.logStatistics(perfstats);
  // Update the stats once per second.
  window.setTimeout(updateStatistics_, 1000);
}

/**
 * Entry-point for Me2Me connections, handling showing of the host-upgrade nag
 * dialog if necessary.
 *
 * @param {string} hostId The unique id of the host.
 * @return {void} Nothing.
 */
remoting.connectMe2Me = function(hostId) {
  var host = remoting.hostList.getHostForId(hostId);
  if (!host) {
    showConnectError_(remoting.Error.HOST_IS_OFFLINE);
    return;
  }
  var webappVersion = chrome.runtime.getManifest().version;
  if (remoting.Host.needsUpdate(host, webappVersion)) {
    var needsUpdateMessage =
        document.getElementById('host-needs-update-message');
    l10n.localizeElementFromTag(needsUpdateMessage,
                                /*i18n-content*/'HOST_NEEDS_UPDATE_TITLE',
                                host.hostName);
    /** @type {Element} */
    var connect = document.getElementById('host-needs-update-connect-button');
    /** @type {Element} */
    var cancel = document.getElementById('host-needs-update-cancel-button');
    /** @param {Event} event */
    var onClick = function(event) {
      connect.removeEventListener('click', onClick, false);
      cancel.removeEventListener('click', onClick, false);
      if (event.target == connect) {
        remoting.connectMe2MeHostVersionAcknowledged_(host);
      } else {
        window.location.replace(chrome.extension.getURL('main.html'));
      }
    }
    connect.addEventListener('click', onClick, false);
    cancel.addEventListener('click', onClick, false);
    remoting.setMode(remoting.AppMode.CLIENT_HOST_NEEDS_UPGRADE);
  } else {
    remoting.connectMe2MeHostVersionAcknowledged_(host);
  }
};

/**
 * Shows PIN entry screen localized to include the host name, and registers
 * a host-specific one-shot event handler for the form submission.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @return {void} Nothing.
 */
remoting.connectMe2MeHostVersionAcknowledged_ = function(host) {
  remoting.connector = new remoting.SessionConnector(
      document.getElementById('session-mode'),
      remoting.onConnected,
      showConnectError_);
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);

  /** @param {function(string):void} onPinFetched */
  var requestPin = function(onPinFetched) {
    /** @type {Element} */
    var pinForm = document.getElementById('pin-form');
    /** @param {Event} event */
    var onSubmit = function(event) {
      event.preventDefault();
      pinForm.removeEventListener('submit', onSubmit, false);
      var pin = document.getElementById('pin-entry').value;
      remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
      onPinFetched(pin);
    };
    pinForm.addEventListener('submit', onSubmit, false);

    var message = document.getElementById('pin-message');
    l10n.localizeElement(message, host.hostName);
    remoting.setMode(remoting.AppMode.CLIENT_PIN_PROMPT);
  };
  remoting.connector.connectMe2Me(host, requestPin);
};

/** @param {remoting.ClientSession} clientSession */
remoting.onConnected = function(clientSession) {
  remoting.connector = null;
  remoting.clientSession = clientSession;
  remoting.clientSession.setOnStateChange(onClientStateChange_);
  setConnectionInterruptedButtonsText_();
  var connectedTo = document.getElementById('connected-to');
  connectedTo.innerText = clientSession.hostDisplayName;
  remoting.setMode(remoting.AppMode.IN_SESSION);
  remoting.toolbar.center();
  remoting.toolbar.preview();
  remoting.clipboard.startSession();
  updateStatistics_();
};
