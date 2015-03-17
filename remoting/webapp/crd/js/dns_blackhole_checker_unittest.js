// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * TODO(garykac): Create interface for SignalStrategy.
 * @suppress {checkTypes|checkVars|reportUnknownTypes|visibility}
 */

(function() {

'use strict';

/** @type {(sinon.Spy|function(remoting.SignalStrategy.State))} */
var onStateChange = null;

/** @type {(sinon.Spy|function(Element):void)} */
var onIncomingStanzaCallback = null;

/** @type {remoting.DnsBlackholeChecker} */
var checker = null;

/** @type {remoting.MockSignalStrategy} */
var signalStrategy = null;
var fakeXhrs;

QUnit.module('dns_blackhole_checker', {
  beforeEach: function(assert) {
    fakeXhrs = [];
    sinon.useFakeXMLHttpRequest().onCreate = function(xhr) {
      fakeXhrs.push(xhr);
    };

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

    assert.equal(fakeXhrs.length, 1, 'exactly one XHR is issued');
    assert.equal(
        fakeXhrs[0].url, remoting.DnsBlackholeChecker.URL_TO_REQUEST_,
        'the correct URL is requested');
  },
  afterEach: function() {
    base.dispose(checker);
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.CLOSED);

    onStateChange = null;
    onIncomingStanzaCallback = null;
    checker = null;
  },
});

QUnit.test('success',
  function(assert) {
    fakeXhrs[0].respond(200);
    sinon.assert.notCalled(onStateChange);

    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
      remoting.SignalStrategy.State.CONNECTED
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.calledWith(onStateChange, state);
      assert.equal(checker.getState(), state);
    });
  }
);

QUnit.test('http response after connected',
  function(assert) {
    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.calledWith(onStateChange, state);
      assert.equal(checker.getState(), state);
    });
    onStateChange.reset();

    // Verify that DnsBlackholeChecker stays in HANDSHAKE state even if the
    // signal strategy has connected.
    signalStrategy.setStateForTesting(remoting.SignalStrategy.State.CONNECTED);
    sinon.assert.notCalled(onStateChange);
    assert.equal(checker.getState(), remoting.SignalStrategy.State.HANDSHAKE);

    // Verify that DnsBlackholeChecker goes to CONNECTED state after the
    // the HTTP request has succeeded.
    fakeXhrs[0].respond(200);
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.CONNECTED);
  }
);

QUnit.test('connect failed',
  function(assert) {
    fakeXhrs[0].respond(200);
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

QUnit.test('blocked',
  function(assert) {
    fakeXhrs[0].respond(400);
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.FAILED);
    assert.equal(checker.getError().getTag(),
                 remoting.Error.Tag.NOT_AUTHORIZED);
    onStateChange.reset();

    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
      remoting.SignalStrategy.State.CONNECTED
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.notCalled(onStateChange);
      assert.equal(checker.getState(), remoting.SignalStrategy.State.FAILED);
    });
  }
);

QUnit.test('blocked after connected',
  function(assert) {
    [
      remoting.SignalStrategy.State.CONNECTING,
      remoting.SignalStrategy.State.HANDSHAKE,
    ].forEach(function(state) {
      signalStrategy.setStateForTesting(state);
      sinon.assert.calledWith(onStateChange, state);
      assert.equal(checker.getState(), state);
    });
    onStateChange.reset();

    // Verify that DnsBlackholeChecker stays in HANDSHAKE state even if the
    // signal strategy has connected.
    signalStrategy.setStateForTesting(remoting.SignalStrategy.State.CONNECTED);
    sinon.assert.notCalled(onStateChange);
    assert.equal(checker.getState(), remoting.SignalStrategy.State.HANDSHAKE);

    // Verify that DnsBlackholeChecker goes to FAILED state after it gets the
    // blocked HTTP response.
    fakeXhrs[0].respond(400);
    sinon.assert.calledWith(onStateChange,
                            remoting.SignalStrategy.State.FAILED);
    assert.ok(checker.getError().hasTag(remoting.Error.Tag.NOT_AUTHORIZED));
  }
);

})();
