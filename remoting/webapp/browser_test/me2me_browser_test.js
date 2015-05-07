// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @constructor */
browserTest.RetryOnHostOffline = function() {
  /** @private */
  this.mockConnection_ = new remoting.MockConnection();

  // Fake an host offline error on first connect.
  var plugin = this.mockConnection_.plugin();
  var State = remoting.ClientSession.State;
  var Error = remoting.ClientSession.ConnectionError;
  var that = this;

  plugin.mock$onConnect().then(function() {
    plugin.mock$setConnectionStatus(State.CONNECTING);
  }).then(function() {
    plugin.mock$setConnectionStatus(State.FAILED, Error.HOST_IS_OFFLINE);
  }).then(function() {
    that.cleanup_();
    that.mockConnection_ = new remoting.MockConnection();
    var newPlugin = that.mockConnection_.plugin();
    // Let the second connect succeed.
    newPlugin.mock$useDefaultBehavior(remoting.MockClientPlugin.AuthMethod.PIN);
  });
};

/** @private */
browserTest.RetryOnHostOffline.prototype.cleanup_ = function() {
  this.mockConnection_.restore();
  this.mockConnection_ = null;
};

browserTest.RetryOnHostOffline.prototype.run = function() {
  var that = this;
  browserTest.connectMe2Me().then(function() {
    return browserTest.enterPIN('123456');
  }).then(function() {
    return browserTest.disconnect();
  }).then(function() {
    that.cleanup_();
    browserTest.pass();
  }).catch(function(/** Error */ reason) {
    that.cleanup_();
    browserTest.fail(reason);
  });
};

})();
