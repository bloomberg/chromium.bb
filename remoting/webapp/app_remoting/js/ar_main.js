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
  remoting.app = new remoting.Application(remoting.app_capabilities());
  var app_remoting = new remoting.AppRemoting(remoting.app);
  remoting.app.start();
};

window.addEventListener('load', remoting.startAppRemoting, false);
