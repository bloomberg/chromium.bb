// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function(){

'use strict';

/**
 * The background service is responsible for listening to incoming connection
 * requests from Hangouts and the webapp.
 *
 * @param {remoting.AppLauncher} appLauncher
 */
function initializeBackgroundService(appLauncher) {
  function initializeIt2MeService() {
    /** @type {remoting.It2MeService} */
    remoting.it2meService = new remoting.It2MeService(appLauncher);
    remoting.it2meService.init();
  }

  chrome.runtime.onSuspend.addListener(function() {
    base.debug.assert(remoting.it2meService != null);
    remoting.it2meService.dispose();
    remoting.it2meService = null;
  });

  remoting.settings = new remoting.Settings();

  chrome.runtime.onSuspendCanceled.addListener(initializeIt2MeService);
  initializeIt2MeService();
}

function main() {
  if (base.isAppsV2()) {
    new remoting.ActivationHandler(base.Ipc.getInstance(),
                                   new remoting.V2AppLauncher());
  }
}

remoting.enableHangoutsRemoteAssistance = function() {
  /** @type {remoting.AppLauncher} */
  var appLauncher = base.isAppsV2() ? new remoting.V1AppLauncher():
                                      new remoting.V2AppLauncher();
  initializeBackgroundService(appLauncher);
};

window.addEventListener('load', main, false);

}());
