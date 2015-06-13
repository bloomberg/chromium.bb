// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Entry point ('load' handler) for App Remoting webapp.
 */
remoting.startAppRemoting = function() {
  // This string is overwritten with the actual app id by the build script.
  var appId = 'APP_REMOTING_APPLICATION_ID';

  // This string is overwritten with the actual capabilities required by
  // this application.
  var capabilities = ['APPLICATION_CAPABILITIES'];

  remoting.app = new remoting.AppRemoting(appId, capabilities);
  remoting.app.start();
};

window.addEventListener('load', remoting.startAppRemoting, false);
