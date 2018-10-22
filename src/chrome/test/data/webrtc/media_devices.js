/**
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Queries for media devices on the current system using the getMediaDevices
 * API.
 *
 * Returns the list of devices available.
 */
function enumerateDevices() {
  navigator.mediaDevices.enumerateDevices().then(function(devices) {
    returnToTest(JSON.stringify(devices));
  });
}
