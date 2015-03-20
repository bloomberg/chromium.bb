// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

remoting.initElementEventHandlers = function() {
  var goHome = function() {
    remoting.setMode(remoting.AppMode.HOME);
  };
  var goEnterAccessCode = function() {
    // We don't need a token until we authenticate, but asking for one here
    // handles the token-expired case earlier, avoiding asking the user for
    // the access code both before and after re-authentication.
    remoting.identity.getToken().
        then(function(token) {
          remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
        }).
        catch(remoting.Error.handler(remoting.showErrorMessage));
  };
  var goFinishedIT2Me = function() {
    if (remoting.currentMode == remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME) {
      remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
    } else {
      goHome();
    }
  };
  /** @param {Event} event The event. */
  var sendAccessCode = function(event) {
    remoting.connectIT2Me();
    event.preventDefault();
  };
  var reconnect = function() {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
    remoting.app.getSessionConnector().reconnect();
  };
  var cancelAccessCode = function() {
    remoting.setMode(remoting.AppMode.HOME);
    document.getElementById('access-code-entry').value = '';
  };
  /** @type {Array<{event: string, id: string, fn: function(Event):void}>} */
  var it2me_actions = [
      { event: 'click', id: 'access-mode-button', fn: goEnterAccessCode },
      { event: 'submit', id: 'access-code-form', fn: sendAccessCode },
      { event: 'click', id: 'cancel-access-code-button', fn: cancelAccessCode},
      { event: 'click', id: 'cancel-share-button', fn: remoting.cancelShare },
      { event: 'click', id: 'client-finished-it2me-button', fn: goHome },
      { event: 'click', id: 'get-started-it2me',
        fn: remoting.showIT2MeUiAndSave },
      { event: 'click', id: 'host-finished-button', fn: goHome },
      { event: 'click', id: 'share-button', fn: remoting.tryShare }
  ];
  /** @type {Array<{event: string, id: string, fn: function(Event):void}>} */
  var me2me_actions = [
      { event: 'click', id: 'client-finished-me2me-button', fn: goHome },
      { event: 'click', id: 'client-reconnect-button', fn: reconnect },
      { event: 'click', id: 'daemon-pin-cancel', fn: goHome },
      { event: 'click', id: 'get-started-me2me',
        fn: remoting.showMe2MeUiAndSave }
  ];
  /** @type {Array<{event: string, id: string, fn: function(Event):void}>} */
  var host_actions = [
      { event: 'click', id: 'close-paired-client-manager-dialog', fn: goHome },
      { event: 'click', id: 'host-config-done-dismiss', fn: goHome },
      { event: 'click', id: 'host-config-error-dismiss', fn: goHome },
      { event: 'click', id: 'open-paired-client-manager-dialog',
        fn: remoting.setMode.bind(null,
                                  remoting.AppMode.HOME_MANAGE_PAIRINGS) },
      { event: 'click', id: 'stop-sharing-button', fn: remoting.cancelShare }
  ];
  /** @type {Array<{event: string, id: string, fn: function(Event):void}>} */
  var auth_actions = [
      { event: 'click', id: 'cancel-connect-button', fn: goHome },
      { event: 'click', id: 'sign-out', fn:remoting.signOut },
      { event: 'click', id: 'token-refresh-error-ok', fn: goHome },
      { event: 'click', id: 'token-refresh-error-sign-in',
        fn: remoting.handleAuthFailureAndRelaunch }
  ];
  registerEventListeners(it2me_actions);
  registerEventListeners(me2me_actions);
  registerEventListeners(host_actions);
  registerEventListeners(auth_actions);
}

/**
 * Sign the user out of Chromoting by clearing (and revoking, if possible) the
 * OAuth refresh token.
 *
 * Also clear all local storage, to avoid leaking information.
 */
remoting.signOut = function() {
  remoting.oauth2.removeCachedAuthToken().then(function(){
    chrome.storage.local.clear();
    remoting.setMode(remoting.AppMode.HOME);
    window.location.reload();
  });
};

