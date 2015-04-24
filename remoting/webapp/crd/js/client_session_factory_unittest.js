// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @type {remoting.MockConnection} */
var mockConnection;
/** @type {remoting.ClientSessionFactory} */
var factory;
/** @type {remoting.ClientSession.EventHandler} */
var listener;

/**
 * @constructor
 * @implements {remoting.ClientSession.EventHandler}
 */
var SessionListener = function() {};
SessionListener.prototype.onConnectionFailed = function(error) {};
SessionListener.prototype.onConnected = function(connectionInfo) {};
SessionListener.prototype.onDisconnected = function(reason) {};
SessionListener.prototype.onError = function(error) {};

QUnit.module('ClientSessionFactory', {
  beforeEach: function() {
    chromeMocks.activate(['identity']);
    chromeMocks.identity.mock$setToken('fake_token');

    mockConnection = new remoting.MockConnection();
    listener = new SessionListener();
    factory = new remoting.ClientSessionFactory(
        document.createElement('div'),
        [remoting.ClientSession.Capability.VIDEO_RECORDER]);
  },
  afterEach: function() {
    mockConnection.restore();
    chromeMocks.restore();
  }
});

QUnit.test('createSession() should return a remoting.ClientSession',
    function(assert) {
  return factory.createSession(listener).then(
    function(/** remoting.ClientSession */ session){
      assert.ok(session instanceof remoting.ClientSession);
      assert.ok(
          mockConnection.plugin().hasCapability(
              remoting.ClientSession.Capability.VIDEO_RECORDER),
          'Capability is set correctly.');
  });
});

QUnit.test('createSession() should reject on signal strategy failure',
    function(assert) {
  var mockSignalStrategy = mockConnection.signalStrategy();
  mockSignalStrategy.connect = function() {
    Promise.resolve().then(function () {
      mockSignalStrategy.setStateForTesting(
          remoting.SignalStrategy.State.FAILED);
    });
  };

  var signalStrategyDispose = sinon.stub(mockSignalStrategy, 'dispose');

  return factory.createSession(listener).then(
    assert.ok.bind(assert, false, 'Expect createSession() to fail.')
  ).catch(function(/** remoting.Error */ error) {
    assert.ok(
        signalStrategyDispose.called, 'SignalStrategy is disposed on failure.');
    assert.equal(error.getDetail(), 'setStateForTesting',
                 'Error message is set correctly.');
  });
});

QUnit.test('createSession() should reject on plugin initialization failure',
    function(assert) {
  var mockSignalStrategy = mockConnection.signalStrategy();
  var plugin = mockConnection.plugin();
  plugin.mock$initializationResult = false;

  var signalStrategyDispose = sinon.stub(mockSignalStrategy, 'dispose');

  return factory.createSession(listener).then(function() {
    assert.ok(false, 'Expect createSession() to fail.');
  }).catch(function(/** remoting.Error */ error) {
    assert.ok(
        signalStrategyDispose.called, 'SignalStrategy is disposed on failure.');
    assert.ok(error.hasTag(remoting.Error.Tag.MISSING_PLUGIN),
        'Initialization failed with MISSING_PLUGIN.');
  });
});

})();
