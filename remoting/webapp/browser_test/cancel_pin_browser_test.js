// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes}
 * Browser test for the scenario below:
 * 1. Attempt to connect.
 * 2. Hit cancel at the PIN prompt.
 * 3. Reconnect with the PIN.
 * 4. Verify that the session is connected.
 */

'use strict';

/** @constructor */
browserTest.Cancel_PIN = function() {};

browserTest.Cancel_PIN.prototype.run = function(data) {
  browserTest.expect(typeof data.pin == 'string');

  var AppMode = remoting.AppMode;
  browserTest.clickOnControl('this-host-connect');
  browserTest.onUIMode(AppMode.CLIENT_PIN_PROMPT).then(function() {
    browserTest.clickOnControl('cancel-pin-entry-button');
    return browserTest.onUIMode(AppMode.HOME);
  }).then(function() {
    browserTest.clickOnControl('this-host-connect');
    return browserTest.onUIMode(AppMode.CLIENT_PIN_PROMPT);
  }).then(
    this.enterPin_.bind(this, data.pin)
  ).then(function() {
    // On fulfilled.
    browserTest.pass();
  }, function(reason) {
    // On rejected.
    browserTest.fail(reason);
  });
};

browserTest.Cancel_PIN.prototype.enterPin_ = function(pin) {
  document.getElementById('pin-entry').value = pin;
  browserTest.clickOnControl('pin-connect-button');
  return browserTest.expectMe2MeConnected();
};
