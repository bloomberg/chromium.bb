// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

function onLoad() {
  var restartWebapp = function() {
    window.location.replace(chrome.extension.getURL('main.html'));
  };
  var goEnterAccessCode = function() {
    // We don't need a token until we authenticate, but asking for one here
    // handles the token-expired case earlier, avoiding asking the user for
    // the access code both before and after re-authentication.
    remoting.identity.callWithToken(
        /** @param {string} token */
        function(token) {
          remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
        },
        remoting.showErrorMessage);
  };
  var goFinishedIt2Me = function() {
    if (remoting.currentMode == remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME) {
      remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
    } else {
      restartWebapp();
    }
  };
  var reload = function() {
    window.location.reload();
  };
  /** @param {Event} event The event. */
  var sendAccessCode = function(event) {
    remoting.connectIt2Me();
    event.preventDefault();
  };
  /** @param {Event} event The event. */
  var connectHostWithPin = function(event) {
    remoting.connectMe2MeWithPin();
    event.preventDefault();
  };
  var doAuthRedirect = function() {
    remoting.oauth2.doAuthRedirect();
  };
  /** @param {Event} event The event. */
  var stopDaemon = function(event) {
    remoting.hostSetupDialog.showForStop();
    event.stopPropagation();
  };
  var cancelAccessCode = function() {
    remoting.setMode(remoting.AppMode.HOME);
    document.getElementById('access-code-entry').value = '';
  };
  /** @type {Array.<{event: string, id: string, fn: function(Event):void}>} */
  var actions = [
      { event: 'click', id: 'sign-out', fn: remoting.signOut },
      { event: 'click', id: 'toolbar-disconnect', fn: remoting.disconnect },
      { event: 'click', id: 'send-ctrl-alt-del',
        fn: remoting.sendCtrlAltDel },
      { event: 'click', id: 'send-print-screen',
        fn: remoting.sendPrintScreen },
      { event: 'click', id: 'auth-button', fn: doAuthRedirect },
      { event: 'click', id: 'share-button', fn: remoting.tryShare },
      { event: 'click', id: 'access-mode-button', fn: goEnterAccessCode },
      { event: 'click', id: 'cancel-share-button', fn: remoting.cancelShare },
      { event: 'click', id: 'stop-sharing-button', fn: remoting.cancelShare },
      { event: 'click', id: 'host-finished-button', fn: restartWebapp },
      { event: 'click', id: 'client-finished-it2me-button',
        fn: goFinishedIt2Me },
      { event: 'click', id: 'client-finished-me2me-button', fn: restartWebapp },
      { event: 'click', id: 'cancel-pin-entry-button', fn: restartWebapp },
      { event: 'click', id: 'client-reconnect-button', fn: reload },
      { event: 'click', id: 'cancel-access-code-button', fn: cancelAccessCode},
      { event: 'click', id: 'cancel-connect-button', fn: restartWebapp },
      { event: 'click', id: 'toolbar-stub',
        fn: function() { remoting.toolbar.toggle(); } },
      { event: 'click', id: 'start-daemon',
        fn: function() { remoting.hostSetupDialog.showForStart(); } },
      { event: 'click', id: 'change-daemon-pin',
        fn: function() { remoting.hostSetupDialog.showForPin(); } },
      { event: 'click', id: 'stop-daemon', fn: stopDaemon },
      { event: 'submit', id: 'access-code-form', fn: sendAccessCode },
      { event: 'submit', id: 'pin-form', fn: connectHostWithPin },
      { event: 'click', id: 'get-started-it2me',
        fn: remoting.showIt2MeUiAndSave },
      { event: 'click', id: 'get-started-me2me',
        fn: remoting.showMe2MeUiAndSave },
      { event: 'click', id: 'daemon-pin-cancel',
        fn: function() { remoting.setMode(remoting.AppMode.HOME); } },
      { event: 'click', id: 'host-config-done-dismiss',
        fn: function() { remoting.setMode(remoting.AppMode.HOME); } },
      { event: 'click', id: 'host-config-error-dismiss',
        fn: function() { remoting.setMode(remoting.AppMode.HOME); } },
      { event: 'click', id: 'host-config-install-continue',
        fn: function() { remoting.hostSetupDialog.onInstallDialogOk(); } },
      { event: 'click', id: 'host-config-install-dismiss',
        fn: function() { remoting.hostSetupDialog.hide(); } },
      { event: 'click', id: 'host-config-install-retry', fn: function() {
          remoting.hostSetupDialog.onInstallDialogRetry(); } },
      { event: 'click', id: 'token-refresh-error-ok',
        fn: function() { remoting.setMode(remoting.AppMode.HOME); } },
      { event: 'click', id: 'token-refresh-error-sign-in', fn: doAuthRedirect }
  ];

  for (var i = 0; i < actions.length; ++i) {
    var action = actions[i];
    var element = document.getElementById(action.id);
    if (element) {
      element.addEventListener(action.event, action.fn, false);
    } else {
      console.error('Could not set ' + action.event +
                    ' event handler on element ' + action.id +
                    ': element not found.');
    }
  }
  remoting.init();
}

function onBeforeUnload() {
  return remoting.promptClose();
}

window.addEventListener('load', onLoad, false);
window.addEventListener('beforeunload', onBeforeUnload, false);
window.addEventListener('resize', remoting.onResize, false);
window.addEventListener('unload', remoting.disconnect, false);
