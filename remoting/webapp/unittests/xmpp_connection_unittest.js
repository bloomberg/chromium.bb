// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var testUsername = 'testUsername@gmail.com';
var testToken = 'testToken';
var socketId = 3;

/** @type {(sinon.Spy|function(remoting.SignalStrategy.State):void)} */
var onStateChange = function() {};

var onStanzaStr = null;

/** @type {remoting.XmppConnection} */
var connection = null;

module('XmppConnection', {
  setup: function() {
    onStateChange = sinon.spy();
    onStanzaStr = sinon.spy();
    /** @param {Element} stanza */
    function onStanza(stanza) {
      onStanzaStr(new XMLSerializer().serializeToString(stanza));
    }

    sinon.$setupStub(chrome.socket, 'create');
    sinon.$setupStub(chrome.socket, 'connect');
    sinon.$setupStub(chrome.socket, 'write');
    sinon.$setupStub(chrome.socket, 'read');
    sinon.$setupStub(chrome.socket, 'destroy');
    sinon.$setupStub(chrome.socket, 'secure');

    connection = new remoting.XmppConnection();
    connection.setStateChangedCallback(
        /** @type {function(remoting.SignalStrategy.State):void} */
            (onStateChange));
    connection.setIncomingStanzaCallback(onStanza);
  },

  teardown: function() {
    chrome.socket.create.$testStub.restore();
    chrome.socket.connect.$testStub.restore();
    chrome.socket.write.$testStub.restore();
    chrome.socket.read.$testStub.restore();
    chrome.socket.destroy.$testStub.restore();
    chrome.socket.secure.$testStub.restore();
  }
});

test('should go to FAILED state when failed to connect', function() {
  connection.connect(
      'xmpp.example.com:123', 'testUsername@gmail.com', 'testToken');
  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.CONNECTING);
  sinon.assert.calledWith(chrome.socket.create, "tcp", {});
  chrome.socket.create.$testStub.getCall(0).args[2]({socketId: socketId});

  sinon.assert.calledWith(
      chrome.socket.connect, socketId, "xmpp.example.com", 123);
  chrome.socket.connect.$testStub.getCall(0).args[3](-1);

  QUnit.equal(connection.getError().tag, remoting.Error.Tag.NETWORK_FAILURE);
});

test('should use XmppLoginHandler to complete handshake and read data',
     function() {
  connection.connect(
      'xmpp.example.com:123', 'testUsername@gmail.com', 'testToken');
  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.CONNECTING);
  sinon.assert.calledWith(chrome.socket.create, "tcp", {});
  chrome.socket.create.$testStub.getCall(0).args[2]({socketId: socketId});

  sinon.assert.calledWith(
      chrome.socket.connect, socketId, "xmpp.example.com", 123);
  chrome.socket.connect.$testStub.getCall(0).args[3](0);

  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.HANDSHAKE);

  var parser = new remoting.XmppStreamParser();
  var parserMock = sinon.mock(parser);
  var setCallbacksCalled = parserMock.expects('setCallbacks').once();
  var handshakeDoneCallback =
      connection.loginHandler_.getHandshakeDoneCallbackForTesting();
  handshakeDoneCallback('test@example.com/123123', parser);
  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.CONNECTED);
  setCallbacksCalled.verify();

  // Simulate read() callback with |data|. It should be passed to the parser.
  var data = base.encodeUtf8('<iq id="1">hello</iq>');
  sinon.assert.calledWith(chrome.socket.read, socketId);
  var appendDataCalled = parserMock.expects('appendData').once().withArgs(data);
  chrome.socket.read.$testStub.getCall(0).args[1]({resultCode: 0, data: data});
  appendDataCalled.verify();
});

})();
