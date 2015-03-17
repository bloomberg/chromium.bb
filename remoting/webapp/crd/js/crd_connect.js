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
  remoting.It2MeConnectFlow.start(connector, accessCode).catch(
    function(/** remoting.Error */ reason){
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
  var connector = remoting.app.getSessionConnector();
  var flow = new remoting.Me2MeConnectFlow(connector, host);
  flow.start();
};