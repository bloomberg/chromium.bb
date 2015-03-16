// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var testUsername = 'testUsername@gmail.com';
var testToken = 'testToken';
var socketId = 3;

var onStanzaStr = null;

/** @type {remoting.XmppConnection} */
var connection = null;

/** @type {remoting.TcpSocket} */
var socket = null;

var stateChangeHandler = function(/** remoting.SignalStrategy.State */ state) {}

function onStateChange(/** remoting.SignalStrategy.State */ state) {
  stateChangeHandler(state)
};

/** @returns {Promise} */
function expectNextState(/** remoting.SignalStrategy.State */ expectedState) {
  return new Promise(function(resolve, reject) {
    stateChangeHandler = function(/** remoting.SignalStrategy.State */ state) {
      QUnit.equal(state, expectedState);
      QUnit.equal(connection.getState(), expectedState);
      resolve(0);
    }
  });
}

module('XmppConnection', {
  setup: function() {
    onStanzaStr = sinon.spy();
    /** @param {Element} stanza */
    function onStanza(stanza) {
      onStanzaStr(new XMLSerializer().serializeToString(stanza));
    }

    socket = /** @type{remoting.TcpSocket} */
        (sinon.createStubInstance(remoting.TcpSocket));

    connection = new remoting.XmppConnection();
    connection.setSocketForTests(socket);
    connection.setStateChangedCallback(onStateChange);
    connection.setIncomingStanzaCallback(onStanza);
  }
});

QUnit.asyncTest('should go to FAILED state when failed to connect', function() {
  $testStub(socket.connect).withArgs("xmpp.example.com", 123)
      .returns(new Promise(function(resolve, reject) { reject(-1); }));

  var deferredSend = new base.Deferred();
  $testStub(socket.send).onFirstCall().returns(deferredSend.promise());

  expectNextState(remoting.SignalStrategy.State.CONNECTING).then(onConnecting);
  connection.connect('xmpp.example.com:123', 'testUsername@gmail.com',
                     'testToken');

  function onConnecting() {
    expectNextState(remoting.SignalStrategy.State.FAILED).then(onFailed);
  }

  function onFailed() {
    sinon.assert.calledWith(socket.dispose);
    QUnit.ok(connection.getError().hasTag(remoting.Error.Tag.NETWORK_FAILURE));

    QUnit.start();
  }
});

QUnit.asyncTest('should use XmppLoginHandler for handshake', function() {
  $testStub(socket.connect).withArgs("xmpp.example.com", 123)
      .returns(new Promise(function(resolve, reject) { resolve(0) }));

  var deferredSend = new base.Deferred();
  $testStub(socket.send).onFirstCall().returns(deferredSend.promise());

  var parser = new remoting.XmppStreamParser();
  var parserMock = sinon.mock(parser);
  var setCallbacksCalled = parserMock.expects('setCallbacks').once();

  expectNextState(remoting.SignalStrategy.State.CONNECTING).then(onConnecting);
  connection.connect(
      'xmpp.example.com:123', 'testUsername@gmail.com', 'testToken');

  function onConnecting() {
    expectNextState(remoting.SignalStrategy.State.HANDSHAKE).then(onHandshake);
  }

  function onHandshake() {
    var handshakeDoneCallback =
        connection.loginHandler_.getHandshakeDoneCallbackForTesting();

    expectNextState(remoting.SignalStrategy.State.CONNECTED).then(onConnected);
    handshakeDoneCallback('test@example.com/123123', parser);
  }

  function onConnected() {
    setCallbacksCalled.verify();

    // Simulate read() callback with |data|. It should be passed to
    // the parser.
    var data = base.encodeUtf8('<iq id="1">hello</iq>');
    sinon.assert.calledWith(socket.startReceiving);
    var appendDataCalled =
        parserMock.expects('appendData').once().withArgs(data);
    $testStub(socket.startReceiving).getCall(0).args[0](data);
    appendDataCalled.verify();

    QUnit.start();
  }
});

})();
