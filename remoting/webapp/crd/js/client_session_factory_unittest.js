// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var originalPluginFactory;
var originalIdentity;
var originalSettings;

/** @type {remoting.MockSignalStrategy} */
var mockSignalStrategy;
/** @type {remoting.ClientSessionFactory} */
var factory;
/** @type {remoting.ClientSession.EventHandler} */
var listener;
/** @type {sinon.TestStub} */
var createSignalStrategyStub;

/**
 * @constructor
 * @implements {remoting.ClientSession.EventHandler}
 */
var SessionListener = function() {};
SessionListener.prototype.onConnectionFailed = function(error) {};
SessionListener.prototype.onConnected = function(connectionInfo) {};
SessionListener.prototype.onDisconnected = function() {};
SessionListener.prototype.onError = function(error) {};

QUnit.module('ClientSessionFactory', {
  beforeEach: function() {
    originalPluginFactory = remoting.ClientPlugin.factory;
    remoting.ClientPlugin.factory = new remoting.MockClientPluginFactory();

    mockSignalStrategy = new remoting.MockSignalStrategy(
        'jid', remoting.SignalStrategy.Type.XMPP);
    createSignalStrategyStub = sinon.stub(remoting.SignalStrategy, 'create');
    createSignalStrategyStub.returns(mockSignalStrategy);
    listener = new SessionListener();

    originalIdentity = remoting.identity;
    remoting.identity = new remoting.Identity();
    chromeMocks.activate(['identity']);
    chromeMocks.identity.mock$setToken('fake_token');

    originalSettings = remoting.settings;
    remoting.settings = new remoting.Settings();

    remoting.identity.getUserInfo = function() {
      return { email: 'email', userName: 'userName'};
    };

    factory = new remoting.ClientSessionFactory(
      document.createElement('div'), ['fake_capability']);
  },
  afterEach: function() {
    remoting.settings = originalSettings;
    remoting.identity = originalIdentity;
    chromeMocks.restore();
    remoting.identity = null;
    remoting.ClientPlugin.factory = originalPluginFactory;
    createSignalStrategyStub.restore();
  }
});

QUnit.test('createSession() should return a remoting.ClientSession',
    function(assert) {

  mockSignalStrategy.connect = function() {
    mockSignalStrategy.setStateForTesting(
        remoting.SignalStrategy.State.CONNECTED);
  };

  return factory.createSession(listener).then(
    function(/** remoting.ClientSession */ session){
      assert.ok(session instanceof remoting.ClientSession);
  });
});

QUnit.test('createSession() should reject on signal strategy failure',
    function(assert) {

  mockSignalStrategy.connect = function() {
    mockSignalStrategy.setStateForTesting(remoting.SignalStrategy.State.FAILED);
  };

  var signalStrategyDispose =
      /** @type {sinon.Spy} */ (sinon.spy(mockSignalStrategy, 'dispose'));

  return factory.createSession(listener).then(
    assert.ok.bind(assert, false, 'Expect createSession to fail')
  ).catch(function(/** remoting.Error */ error) {
    assert.ok(signalStrategyDispose.called);
    assert.equal(error.getDetail(), 'setStateForTesting');
  });
});

})();
