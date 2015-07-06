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
  var options = {
    // This string is overwritten with the actual app id by the build script.
    appId: 'APP_REMOTING_APPLICATION_ID',

    // This string is overwritten with the actual capabilities required by
    // this application.
    appCapabilities: ['APPLICATION_CAPABILITIES'],
  };

  var app = new remoting.AppRemoting(options);
  app.start();
};

window.addEventListener('load', remoting.startAppRemoting, false);
