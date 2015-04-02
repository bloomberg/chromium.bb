// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Experimental features that can be accessed via the Chrome debugging console.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

remoting.experimental = {};

/**
 * Sets and stores the scale factor to apply to host sizing requests.
 * The desktopScale applies to the dimensions reported to the host, not
 * to the client DPI reported to it.
 *
 * @param {number} desktopScale Scale factor to apply.
 */
remoting.experimental.setDesktopScale = function(desktopScale) {
  var mode = remoting.app.getConnectionMode();
  if (mode == remoting.Application.Mode.IT2ME ||
      mode == remoting.Application.Mode.ME2ME) {
    var drApp = /** @type {remoting.DesktopRemoting} */ (remoting.app);
    var connectedView = drApp.getConnectedViewForTesting();
    var viewport = connectedView.getViewportForTesting();
    viewport.setDesktopScale(desktopScale);
  }
};
