// Copyright 2014 The Chromium Authors. All rights reserved.
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
 * Initiate an IT2Me connection.
 */
remoting.connectIT2Me = function() {
  var connector = remoting.app.getSessionConnector();
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
  remoting.It2MeConnectFlow.start(connector, accessCode).
    catch(function(/** !remoting.Error */ reason){
      var errorDiv = document.getElementById('connect-error-message');
      l10n.localizeElementFromTag(errorDiv, reason.getTag());
      remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
  });
};

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
    remoting.app.onError(new remoting.Error(
        remoting.Error.Tag.HOST_IS_OFFLINE));
    return;
  }
  var webappVersion = chrome.runtime.getManifest().version;
  var needsUpdateDialog = new remoting.HostNeedsUpdateDialog(
      document.getElementById('host-needs-update-dialog'), host);

  needsUpdateDialog.showIfNecessary(webappVersion).then(function() {
    remoting.connectMe2MeHostVersionAcknowledged_(host);
  }).catch(function(/** remoting.Error */ error) {
    if (error.hasTag(remoting.Error.Tag.CANCELLED)) {
      remoting.setMode(remoting.AppMode.HOME);
    }
  });
};

/**
 * Shows PIN entry screen localized to include the host name, and registers
 * a host-specific one-shot event handler for the form submission.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @return {void} Nothing.
 */
remoting.connectMe2MeHostVersionAcknowledged_ = function(host) {
  /** @type {remoting.SessionConnector} */
  var connector = remoting.app.getSessionConnector();
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);

  /**
   * @param {string} tokenUrl Token-issue URL received from the host.
   * @param {string} hostPublicKey Host public key (DER and Base64 encoded).
   * @param {string} scope OAuth scope to request the token for.
   * @param {function(string, string):void} onThirdPartyTokenFetched Callback.
   */
  var fetchThirdPartyToken = function(
      tokenUrl, hostPublicKey, scope, onThirdPartyTokenFetched) {
    var thirdPartyTokenFetcher = new remoting.ThirdPartyTokenFetcher(
        tokenUrl, hostPublicKey, scope, host.tokenUrlPatterns,
        onThirdPartyTokenFetched);
    thirdPartyTokenFetcher.fetchToken();
  };

  /**
   * @param {boolean} supportsPairing
   * @param {function(string):void} onPinFetched
   */
  var requestPin = function(supportsPairing, onPinFetched) {
    var pinDialog =
        new remoting.PinDialog(document.getElementById('pin-dialog'), host);
    pinDialog.show(supportsPairing).then(function(/** string */ pin) {
      onPinFetched(pin);
      /** @type {boolean} */
      remoting.pairingRequested = pinDialog.pairingRequested();
    });
  };

  /** @param {Object} settings */
  var connectMe2MeHostSettingsRetrieved = function(settings) {
    /** @type {string} */
    var clientId = '';
    /** @type {string} */
    var sharedSecret = '';
    var pairingInfo = /** @type {Object} */ (settings['pairingInfo']);
    if (pairingInfo) {
      clientId = /** @type {string} */ (pairingInfo['clientId']);
      sharedSecret = /** @type {string} */ (pairingInfo['sharedSecret']);
    }
    connector.connectMe2Me(host, requestPin, fetchThirdPartyToken,
                                 clientId, sharedSecret);
  }

  remoting.HostSettings.load(host.hostId, connectMe2MeHostSettingsRetrieved);
};
