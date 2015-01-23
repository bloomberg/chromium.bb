// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var onStateChange = null;
var onIncomingStanzaCallback = null;
var checker = null;
var signalStrategy = null;

module('dns_blackhole_checker', {
  setup: function() {
    sinon.stub(remoting.xhr, 'get');

    onStateChange = sinon.spy();
    onIncomingStanzaCallback = sinon.spy();
    signalStrategy = new remoting.MockSignalStrategy();
    checker = new remoting.DnsBlackholeChecker(signalStrategy);

    checker.setStateChangedCallback(onStateChange);
    checker.setIncomingStanzaCallback(onIncomingStanzaCallback);

    sinon.assert.notCalled(onStateChange);
    sinon.assert.notCalled(signalStrategy.connect);
    checker.connect('server', 'username', 'authToken');
    sinon.assert.calledWith(signalStrategy.connect, 'server', 'username',
                            'authToken');

    sinon.assert.calledWith(remoting.xhr.get,
                            remoting.DnsBlackholeChecker.URL_TO_REQUEST_);
  },
  teardown: function() {
    base.dispose(checker);
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.CLOSED);

    onStateChange = null;
    onIncomingStanzaCallback = null;
    checker = null;

    remoting.xhr.get.restore();
  },
});

test('success',
  function() {
    remoting.xhr.get.getCall(0).args[1]({status: 200});
    sinon.assert.notCalled(onStateChange);

    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
      remoting.SignalStrategy.State.CONNECTED
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.calledWith(onStateChange, state);
      equal(checker.getState(), state);
    });
  }
);

test('http response after connected',
  function() {
    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.calledWith(onStateChange, state);
      equal(checker.getState(), state);
    });
    onStateChange.reset();

    // Verify that DnsBlackholeChecker stays in HANDSHAKE state even if the
    // signal strategy has connected.
    signalStrategy.setStateForTesting(remoting.SignalStrategy.State.CONNECTED);
    sinon.assert.notCalled(onStateChange);
    equal(checker.getState(), remoting.SignalStrategy.State.HANDSHAKE);

    // Verify that DnsBlackholeChecker goes to CONNECTED state after the
    // the HTTP request has succeeded.
    remoting.xhr.get.getCall(0).args[1]({status: 200});
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.CONNECTED);
  }
);

test('connect failed',
  function() {
    remoting.xhr.get.getCall(0).args[1]({status: 200});
    sinon.assert.notCalled(onStateChange);

    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.FAILED
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.calledWith(onStateChange, state);
    });
}
);

test('blocked',
  function() {
    remoting.xhr.get.getCall(0).args[1]({status: 400});
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.FAILED);
    equal(checker.getError(), remoting.Error.NOT_AUTHORIZED);
    onStateChange.reset();

    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
      remoting.SignalStrategy.State.CONNECTED
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.notCalled(onStateChange);
      equal(checker.getState(), remoting.SignalStrategy.State.FAILED);
    });
  }
);

test('blocked after connected',
  function() {
    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.calledWith(onStateChange, state);
      equal(checker.getState(), state);
    });
    onStateChange.reset();

    // Verify that DnsBlackholeChecker stays in HANDSHAKE state even if the
    // signal strategy has connected.
    signalStrategy.setStateForTesting(remoting.SignalStrategy.State.CONNECTED);
    sinon.assert.notCalled(onStateChange);
    equal(checker.getState(), remoting.SignalStrategy.State.HANDSHAKE);

    // Verify that DnsBlackholeChecker goes to FAILED state after it gets the
    // blocked HTTP response.
    remoting.xhr.get.getCall(0).args[1]({status: 400});
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.FAILED);
    equal(checker.getError(), remoting.Error.NOT_AUTHORIZED);
  }
);

})();
