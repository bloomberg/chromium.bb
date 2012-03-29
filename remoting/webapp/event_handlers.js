// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

function onLoad() {
  var goHome = function() {
    window.location.replace(chrome.extension.getURL('main.html'));
  };
  var goEnterAccessCode = function() {
    remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
  };
  var goFinishedIt2Me = function() {
    if (remoting.currentMode == remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME) {
      remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
    } else {
      remoting.setMode(remoting.AppMode.HOME);
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
  var cancelPinEntry = function() {
    remoting.initDaemonUi();
  }
  var doAuthRedirect = function() {
    remoting.oauth2.doAuthRedirect();
  };
  var stopDaemon = function() {
    remoting.daemonPlugin.stop();
    remoting.daemonPlugin.updateDom();
  };
  /** @type {Array.<{event: string, id: string, fn: function(Event):void}>} */
  var actions = [
      { event: 'click', id: 'clear-oauth', fn: remoting.clearOAuth2 },
      { event: 'click', id: 'toolbar-disconnect', fn: remoting.disconnect },
      { event: 'click', id: 'auth-button', fn: doAuthRedirect },
      { event: 'click', id: 'share-button', fn: remoting.tryShare },
      { event: 'click', id: 'access-mode-button', fn: goEnterAccessCode },
      { event: 'click', id: 'cancel-share-button', fn: remoting.cancelShare },
      { event: 'click', id: 'stop-sharing-button', fn: remoting.cancelShare },
      { event: 'click', id: 'host-finished-button', fn: goHome },
      { event: 'click', id: 'client-finished-it2me-button',
        fn: goFinishedIt2Me },
      { event: 'click', id: 'client-finished-me2me-button', fn: goHome },
      { event: 'click', id: 'cancel-pin-entry-button', fn: cancelPinEntry },
      { event: 'click', id: 'client-reconnect-button', fn: reload },
      { event: 'click', id: 'cancel-access-code-button',
        fn: remoting.cancelConnect },
      { event: 'click', id: 'cancel-connect-button',
        fn: remoting.cancelConnect },
      { event: 'click', id: 'toolbar-stub',
        fn: function() { remoting.toolbar.toggle(); } },
      { event: 'click', id: 'start-daemon',
        fn: function() { remoting.askPinDialog.showForStart(); } },
      { event: 'click', id: 'change-daemon-pin',
        fn: function() { remoting.askPinDialog.showForPin(); } },
      { event: 'click', id: 'stop-daemon', fn: stopDaemon },
      { event: 'submit', id: 'access-code-form', fn: sendAccessCode },
      { event: 'submit', id: 'pin-form', fn: connectHostWithPin },
      { event: 'click', id: 'get-started-it2me',
        fn: remoting.showIt2MeUiAndSave },
      { event: 'click', id: 'get-started-me2me',
        fn: remoting.showMe2MeUiAndSave }
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
