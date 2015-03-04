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
 * @type {remoting.ClientSession} The client session object, set once the
 *     connector has invoked its onOk callback.
 */
remoting.clientSession = null;

/**
 * @type {remoting.DesktopConnectedView} The client session object, set once the
 *     connector has invoked its onOk callback.
 */
remoting.desktopConnectedView = null;

/**
 * Update the remoting client layout in response to a resize event.
 *
 * @return {void} Nothing.
 */
remoting.onResize = function() {
  if (remoting.desktopConnectedView) {
    remoting.desktopConnectedView.onResize();
  }
};

/**
 * Handle changes in the visibility of the window, for example by pausing video.
 *
 * @return {void} Nothing.
 */
remoting.onVisibilityChanged = function() {
  if (remoting.desktopConnectedView) {
    remoting.desktopConnectedView.pauseVideo(
      ('hidden' in document) ? document.hidden : document.webkitHidden);
  }
};

/**
 * Disconnect the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.disconnect = function() {
  if (!remoting.clientSession) {
    return;
  }
  remoting.clientSession.disconnect(remoting.Error.NONE);
  console.log('Disconnected.');
};

/**
 * Callback function called when the state of the client plugin changes. The
 * current and previous states are available via the |state| member variable.
 *
 * @param {remoting.ClientSession.StateEvent=} state
 */
function onClientStateChange_(state) {
  switch (state.current) {
    case remoting.ClientSession.State.CLOSED:
      console.log('Connection closed by host');
      if (remoting.desktopConnectedView.getMode() ==
          remoting.DesktopConnectedView.Mode.IT2ME) {
        remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
      } else {
        remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
      }
      remoting.app.onDisconnected();
      break;

    case remoting.ClientSession.State.FAILED:
      var error = remoting.clientSession.getError();
      console.error('Client plugin reported connection failed: ' + error);
      if (error == null) {
        error = remoting.Error.UNEXPECTED;
      }
      remoting.app.onError(error);
      break;

    default:
      console.error('Unexpected client plugin state: ' + state.current);
      // This should only happen if the web-app and client plugin get out of
      // sync, so MISSING_PLUGIN is a suitable error.
      remoting.app.onError(remoting.Error.MISSING_PLUGIN);
      break;
  }

  remoting.clientSession.removeEventListener('stateChanged',
                                             onClientStateChange_);
  remoting.clientSession.cleanup();
  remoting.clientSession = null;
  remoting.desktopConnectedView = null;
}

/**
 * Timer callback to update the statistics panel.
 */
function updateStatistics_() {
  if (!remoting.clientSession ||
      remoting.clientSession.getState() !=
      remoting.ClientSession.State.CONNECTED) {
    return;
  }
  var perfstats = remoting.clientSession.getPerfStats();
  remoting.stats.update(perfstats);
  remoting.clientSession.logStatistics(perfstats);
  // Update the stats once per second.
  window.setTimeout(updateStatistics_, 1000);
}
