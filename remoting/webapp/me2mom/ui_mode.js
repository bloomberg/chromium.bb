// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions related to controlling the modal ui state of the app.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @enum {string} */
remoting.AppMode = {
  HOME: 'home',
  UNAUTHENTICATED: 'auth',
  CLIENT: 'client',
    CLIENT_UNCONNECTED: 'client.unconnected',
    CLIENT_CONNECTING: 'client.connecting',
    CLIENT_CONNECT_FAILED: 'client.connect-failed',
    CLIENT_SESSION_FINISHED: 'client.session-finished',
  HOST: 'host',
    HOST_WAITING_FOR_CODE: 'host.waiting-for-code',
    HOST_WAITING_FOR_CONNECTION: 'host.waiting-for-connection',
    HOST_SHARED: 'host.shared',
    HOST_SHARE_FAILED: 'host.share-failed',
    HOST_SHARE_FINISHED: 'host.share-finished',
  IN_SESSION: 'in-session'
};

/**
 * @type {remoting.AppMode} The current app mode
 */
remoting.currentMode = remoting.AppMode.HOME;

/**
 * Change the app's modal state to |mode|, which is considered to be a dotted
 * hierachy of modes. For example, setMode('host.shared') will show any modal
 * elements with an data-ui-mode attribute of 'host' or 'host.shared' and hide
 * all others.
 *
 * @param {remoting.AppMode} mode The new modal state, expressed as a dotted
 * hiearchy.
 */
remoting.setMode = function(mode) {
  var modes = mode.split('.');
  for (var i = 1; i < modes.length; ++i)
    modes[i] = modes[i - 1] + '.' + modes[i];
  var elements = document.querySelectorAll('[data-ui-mode]');
  for (var i = 0; i < elements.length; ++i) {
    var element = /** @type {Element} */ elements[i];
    var hidden = true;
    for (var m = 0; m < modes.length; ++m) {
      if (hasClass(element.getAttribute('data-ui-mode'), modes[m])) {
        hidden = false;
        break;
      }
    }
    element.hidden = hidden;
  }
  remoting.debug.log('App mode: ' + mode);
  remoting.currentMode = mode;
  if (mode == remoting.AppMode.IN_SESSION) {
    document.removeEventListener('keydown', remoting.DebugLog.onKeydown, false);
  } else {
    document.addEventListener('keydown', remoting.DebugLog.onKeydown, false);
  }
  if (mode == remoting.AppMode.HOME) {
    remoting.hostList.refresh();
  }
};

/**
 * Get the major mode that the app is running in.
 * @return {string} The app's current major mode.
 */
remoting.getMajorMode = function() {
  return remoting.currentMode.split('.')[0];
};
