// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes}
 * Browser test for the scenario below:
 * 1. Change the PIN.
 * 2. Connect with the new PIN.
 * 3. Verify the connection succeeded.
 * 4. Disconnect and reconnect with the old PIN.
 * 5. Verify the connection failed.
 */

'use strict';

/** @constructor */
browserTest.Update_PIN = function() {};

browserTest.Update_PIN.prototype.run = function(data) {
  var LOGIN_BACKOFF_WAIT = 2000;
  // Input validation
  browserTest.expect(typeof data.new_pin == 'string');
  browserTest.expect(typeof data.old_pin == 'string');
  browserTest.expect(data.new_pin != data.old_pin,
                     'The new PIN and the old PIN cannot be the same');

  this.changePIN_(data.new_pin).then(
    browserTest.connectMe2Me
  ).then(
    this.enterPIN_.bind(this, data.old_pin, true /* expectError*/)
  ).then(
    // Sleep for two seconds to allow for the login backoff logic to reset.
    base.Promise.sleep.bind(null, LOGIN_BACKOFF_WAIT)
  ).then(
    browserTest.connectMe2Me
  ).then(
    this.enterPIN_.bind(this, data.new_pin, false /* expectError*/)
  ).then(
    // Clean up the test by disconnecting and changing the PIN back
    this.disconnect_.bind(this)
  ).then(
    // The PIN must be restored regardless of success or failure.
    this.changePIN_.bind(this, data.old_pin),
    this.changePIN_.bind(this, data.old_pin)
  ).then(
    // On fulfilled.
    browserTest.pass,
    // On rejected.
    browserTest.fail
  );
};

browserTest.Update_PIN.prototype.changePIN_ = function(newPin) {
  var AppMode = remoting.AppMode;
  var HOST_RESTART_WAIT = 10000;

  browserTest.clickOnControl('change-daemon-pin');

  return browserTest.onUIMode(AppMode.HOST_SETUP_ASK_PIN).then(function() {
    var onSetupDone = browserTest.onUIMode(AppMode.HOST_SETUP_DONE);
    document.getElementById('daemon-pin-entry').value = newPin;
    document.getElementById('daemon-pin-confirm').value = newPin;
    browserTest.clickOnControl('daemon-pin-ok');
    return onSetupDone;
  }).then(function() {
    browserTest.clickOnControl('host-config-done-dismiss');
    // On Linux, we restart the host after changing the PIN, need to sleep
    // for ten seconds before the host is ready for connection.
    return base.Promise.sleep(HOST_RESTART_WAIT);
  });
};

browserTest.Update_PIN.prototype.disconnect_ = function() {
  var AppMode = remoting.AppMode;

  remoting.disconnect();

  return browserTest.onUIMode(AppMode.CLIENT_SESSION_FINISHED_ME2ME)
  .then(function() {
    var onHome = browserTest.onUIMode(AppMode.HOME);
    browserTest.clickOnControl('client-finished-me2me-button');
    return onHome;
  });
};

browserTest.Update_PIN.prototype.enterPIN_ = function(pin, expectError) {
  // Wait for 500ms before hitting the PIN button. From experiment, sometimes
  // the PIN prompt does not dismiss without the timeout.
  var CONNECT_PIN_WAIT = 500;

  document.getElementById('pin-entry').value = pin;

  return base.Promise.sleep(CONNECT_PIN_WAIT).then(function() {
    browserTest.clickOnControl('pin-connect-button');
  }).then(function() {
    if (expectError) {
      return browserTest.expectMe2MeError(remoting.Error.INVALID_ACCESS_CODE);
    } else {
      return browserTest.expectMe2MeConnected();
    }
  });
};
