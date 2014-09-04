// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var testUsername = 'testUsername@gmail.com';
var testToken = 'testToken';
var socketId = 3;

var onStateChange = null;
var onStanzaStr = null;
var connection = null;

module('XmppConnection', {
  setup: function() {
    onStateChange = sinon.spy();
    onStanzaStr = sinon.spy();
    function onStanza(stanza) {
      onStanzaStr(new XMLSerializer().serializeToString(stanza));
    }

    sinon.stub(chrome.socket, 'create');
    sinon.stub(chrome.socket, 'connect');
    sinon.stub(chrome.socket, 'write');
    sinon.stub(chrome.socket, 'read');
    sinon.stub(chrome.socket, 'destroy');
    sinon.stub(chrome.socket, 'secure');

    connection = new remoting.XmppConnection(onStateChange);
    connection.setIncomingStanzaCallback(onStanza);
  },

  teardown: function() {
    chrome.socket.create.restore();
    chrome.socket.connect.restore();
    chrome.socket.write.restore();
    chrome.socket.read.restore();
    chrome.socket.destroy.restore();
    chrome.socket.secure.restore();
  }
});

test('should go to FAILED state when failed to connect', function() {
  connection.connect(
      'xmpp.example.com:123', 'testUsername@gmail.com', 'testToken');
  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.CONNECTING);
  sinon.assert.calledWith(chrome.socket.create, "tcp", {});
  chrome.socket.create.getCall(0).args[2]({socketId: socketId});

  sinon.assert.calledWith(
      chrome.socket.connect, socketId, "xmpp.example.com", 123);
  chrome.socket.connect.getCall(0).args[3](-1);

  QUnit.equal(connection.getError(), remoting.Error.NETWORK_FAILURE);
});

test('should use XmppLoginHandler to complete handshake and read data',
     function() {
  connection.connect(
      'xmpp.example.com:123', 'testUsername@gmail.com', 'testToken');
  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.CONNECTING);
  sinon.assert.calledWith(chrome.socket.create, "tcp", {});
  chrome.socket.create.getCall(0).args[2]({socketId: socketId});

  sinon.assert.calledWith(
      chrome.socket.connect, socketId, "xmpp.example.com", 123);
  chrome.socket.connect.getCall(0).args[3](0);

  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.HANDSHAKE);

  var parser = new remoting.XmppStreamParser();
  var parserMock = sinon.mock(parser);
  var setCallbacksCalled = parserMock.expects('setCallbacks').once();
  connection.loginHandler_.onHandshakeDoneCallback_('test@example.com/123123',
                                                    parser);
  sinon.assert.calledWith(onStateChange,
                          remoting.SignalStrategy.State.CONNECTED);
  setCallbacksCalled.verify();

  // Simulate read() callback with |data|. It should be passed to the parser.
  var data = base.encodeUtf8('<iq id="1">hello</iq>');
  sinon.assert.calledWith(chrome.socket.read, socketId);
  var appendDataCalled = parserMock.expects('appendData').once().withArgs(data);
  chrome.socket.read.getCall(0).args[1]({resultCode: 0, data: data});
  appendDataCalled.verify();
});

})();
