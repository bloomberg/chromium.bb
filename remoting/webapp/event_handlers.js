// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

function onLoad() {
  var goHome = function() {
    remoting.setMode(remoting.AppMode.HOME);
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
    remoting.connector.reconnect();
  };
  var doAuthRedirect = function() {
    if (!base.isAppsV2()) {
      remoting.oauth2.doAuthRedirect();
    }
  };
  var fixAuthError = function() {
    if (base.isAppsV2()) {
      var onRefresh = function() {
        remoting.hostList.display();
      };
      var refreshHostList = function() {
        goHome();
        remoting.hostList.refresh(onRefresh);
      };
      remoting.identity.removeCachedAuthToken(refreshHostList);
    } else {
      doAuthRedirect();
    }
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
  /** @type {Array.<{event: string, id: string, fn: function(Event):void}>} */
  var me2me_actions = [
      { event: 'click', id: 'change-daemon-pin',
        fn: function() { remoting.hostSetupDialog.showForPin(); } },
      { event: 'click', id: 'client-finished-me2me-button', fn: goHome },
      { event: 'click', id: 'client-reconnect-button', fn: reconnect },
      { event: 'click', id: 'daemon-pin-cancel', fn: goHome },
      { event: 'click', id: 'get-started-me2me',
        fn: remoting.showMe2MeUiAndSave },
      { event: 'click', id: 'start-daemon',
        fn: function() { remoting.hostSetupDialog.showForStart(); } },
      { event: 'click', id: 'stop-daemon', fn: stopDaemon }
  ];
  /** @type {Array.<{event: string, id: string, fn: function(Event):void}>} */
  var host_actions = [
      { event: 'click', id: 'close-paired-client-manager-dialog', fn: goHome },
      { event: 'click', id: 'host-config-done-dismiss', fn: goHome },
      { event: 'click', id: 'host-config-error-dismiss', fn: goHome },
      { event: 'click', id: 'open-paired-client-manager-dialog',
        fn: remoting.setMode.bind(null,
                                  remoting.AppMode.HOME_MANAGE_PAIRINGS) },
      { event: 'click', id: 'stop-sharing-button', fn: remoting.cancelShare }
  ];
  /** @type {Array.<{event: string, id: string, fn: function(Event):void}>} */
  var auth_actions = [
      { event: 'click', id: 'auth-button', fn: doAuthRedirect },
      { event: 'click', id: 'cancel-connect-button', fn: goHome },
      { event: 'click', id: 'sign-out', fn:remoting.signOut },
      { event: 'click', id: 'token-refresh-error-ok', fn: goHome },
      { event: 'click', id: 'token-refresh-error-sign-in', fn: fixAuthError }
  ];
  registerEventListeners(it2me_actions);
  registerEventListeners(me2me_actions);
  registerEventListeners(host_actions);
  registerEventListeners(auth_actions);
  remoting.init();

  window.addEventListener('resize', remoting.onResize, false);
  // When a window goes full-screen, a resize event is triggered, but the
  // Fullscreen.isActive call is not guaranteed to return true until the
  // full-screen event is triggered. In apps v2, the size of the window's
  // client area is calculated differently in full-screen mode, so register
  // for both events.
  remoting.fullscreen.addListener(remoting.onResize);
  if (!base.isAppsV2()) {
    window.addEventListener('beforeunload', remoting.promptClose, false);
    window.addEventListener('unload', remoting.disconnect, false);
  }
}

/**
 * @param {Array.<{event: string, id: string,
 *     fn: function(Event):void}>} actions Array of actions to register.
 */
function registerEventListeners(actions) {
  for (var i = 0; i < actions.length; ++i) {
    var action = actions[i];
    registerEventListener(action.id, action.event, action.fn);
  }
}

/**
 * Add an event listener to the specified element.
 * @param {string} id Id of element.
 * @param {string} eventname Event name.
 * @param {function(Event):void} fn Event handler.
 */
function registerEventListener(id, eventname, fn) {
  var element = document.getElementById(id);
  if (element) {
    element.addEventListener(eventname, fn, false);
  } else {
    console.error('Could not set ' + eventname +
        ' event handler on element ' + id +
        ': element not found.');
  }
}

window.addEventListener('load', onLoad, false);
