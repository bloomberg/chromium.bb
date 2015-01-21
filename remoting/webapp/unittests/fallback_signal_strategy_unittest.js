// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var ControllableSignalStrategy = function(jid) {
  this.jid = jid;
  this.onStateChangedCallback = function() {};
  this.state = null;
  this.onIncomingStanzaCallback = function() {};
  this.dispose = sinon.spy();
  this.connect = sinon.spy();
  this.sendMessage = sinon.spy();
};

ControllableSignalStrategy.prototype.setStateChangedCallback = function(
    onStateChangedCallback) {
  this.onStateChangedCallback =
      onStateChangedCallback ? onStateChangedCallback : function() {};
};

ControllableSignalStrategy.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {
  this.onIncomingStanzaCallback =
      onIncomingStanzaCallback ? onIncomingStanzaCallback
                               : function() {};
};

ControllableSignalStrategy.prototype.getState = function(message) {
  return this.state;
};

ControllableSignalStrategy.prototype.getError = function(message) {
  return remoting.Error.UNKNOWN;
};

ControllableSignalStrategy.prototype.getJid = function(message) {
  return this.jid;
};

ControllableSignalStrategy.prototype.setExternalCallbackForTesting =
    function(externalCallback) {
  this.externalCallback_ = externalCallback;
};

ControllableSignalStrategy.prototype.setStateForTesting =
    function(state, expectExternalCallback) {
  this.state = state;
  this.externalCallback_.reset();
  this.onStateChangedCallback(state);
  if (expectExternalCallback) {
    equal(this.externalCallback_.callCount, 1);
    ok(this.externalCallback_.calledWith(state));
    equal(strategy.getState(), state);
  } else {
    ok(!this.externalCallback_.called);
  }
};

var onStateChange = null;
var onProgressCallback = null;
var onIncomingStanzaCallback = null;
var strategy = null;
var primary = null;
var secondary = null;

module('fallback_signal_strategy', {
  setup: function() {
    onStateChange = sinon.spy();
    onProgressCallback = sinon.spy();
    onIncomingStanzaCallback = sinon.spy();
    strategy = new remoting.FallbackSignalStrategy(
        new ControllableSignalStrategy('primary-jid'),
        new ControllableSignalStrategy('secondary-jid'),
        onProgressCallback);
    strategy.setStateChangedCallback(onStateChange);
    strategy.setIncomingStanzaCallback(onIncomingStanzaCallback);
    primary = strategy.primary_;
    secondary = strategy.secondary_;
    primary.setExternalCallbackForTesting(onStateChange);
    secondary.setExternalCallbackForTesting(onStateChange);
  },
  teardown: function() {
    onStateChange = null;
    onProgressCallback = null;
    onIncomingStanzaCallback = null;
    strategy = null;
    primary = null;
    secondary = null;
  },
  // Assert that the progress callback has been called exactly once
  // since the last call, and with the specified state.
  assertProgress: function(state) {
    ok(onProgressCallback.calledOnce);
    ok(onProgressCallback.calledWith(state));
    onProgressCallback.reset();
  }
});

test('primary succeeds; send & receive routed to it',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    primary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, true);
    primary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, true);

    ok(!onProgressCallback.called);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_SUCCEEDED);
    equal(strategy.getJid(), 'primary-jid');

    ok(!onIncomingStanzaCallback.called);
    primary.onIncomingStanzaCallback('test-receive-primary');
    secondary.onIncomingStanzaCallback('test-receive-secondary');
    ok(onIncomingStanzaCallback.calledOnce);
    ok(onIncomingStanzaCallback.calledWith('test-receive-primary'));

    ok(!primary.sendMessage.called);
    strategy.sendMessage('test-send');
    ok(primary.sendMessage.calledOnce);
    ok(primary.sendMessage.calledWith('test-send'));

    ok(!primary.dispose.called);
    ok(!secondary.dispose.called);
    primary.setStateForTesting(remoting.SignalStrategy.State.CLOSED, true);
    strategy.dispose();
    ok(primary.dispose.calledOnce);
    ok(secondary.dispose.calledOnce);
  }
);

test('primary fails; secondary succeeds; send & receive routed to it',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    primary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, true);

    ok(!onProgressCallback.called);
    ok(!secondary.connect.called);
    primary.setStateForTesting(remoting.SignalStrategy.State.FAILED, false);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_FAILED);

    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, true);

    ok(!onProgressCallback.called);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.SECONDARY_SUCCEEDED);
    equal(strategy.getJid(), 'secondary-jid');

    ok(!onIncomingStanzaCallback.called);
    primary.onIncomingStanzaCallback('test-receive-primary');
    secondary.onIncomingStanzaCallback('test-receive-secondary');
    ok(onIncomingStanzaCallback.calledOnce);
    ok(onIncomingStanzaCallback.calledWith('test-receive-secondary'));

    ok(!secondary.sendMessage.called);
    strategy.sendMessage('test-send');
    ok(!primary.sendMessage.called);
    ok(secondary.sendMessage.calledOnce);
    ok(secondary.sendMessage.calledWith('test-send'));
  }
);

test('primary fails; secondary fails',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    primary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    ok(!onProgressCallback.called);
    ok(!secondary.connect.called);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, true);
    primary.setStateForTesting(remoting.SignalStrategy.State.FAILED, false);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_FAILED);
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.FAILED, true);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.SECONDARY_FAILED);
  }
);

test('primary times out; secondary succeeds',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    primary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, true);
    this.clock.tick(strategy.PRIMARY_CONNECT_TIMEOUT_MS_ - 1);
    ok(!secondary.connect.called);
    ok(!onProgressCallback.called);
    this.clock.tick(1);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_TIMED_OUT);
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, true);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.SECONDARY_SUCCEEDED);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CLOSED, true);
    primary.setStateForTesting(remoting.SignalStrategy.State.FAILED, false);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_FAILED_LATE);
  }
);

test('primary times out; secondary fails',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    primary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, true);
    this.clock.tick(strategy.PRIMARY_CONNECT_TIMEOUT_MS_ - 1);
    ok(!secondary.connect.called);
    ok(!onProgressCallback.called);
    this.clock.tick(1);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_TIMED_OUT);
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.FAILED, true);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.SECONDARY_FAILED);
  }
);

test('primary times out; secondary succeeds; primary succeeds late',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    primary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, true);
    this.clock.tick(strategy.PRIMARY_CONNECT_TIMEOUT_MS_);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_TIMED_OUT);
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, true);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.SECONDARY_SUCCEEDED);
    primary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, false);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, false);
    this.assertProgress(
        remoting.FallbackSignalStrategy.Progress.PRIMARY_SUCCEEDED_LATE);
  }
);

})();
