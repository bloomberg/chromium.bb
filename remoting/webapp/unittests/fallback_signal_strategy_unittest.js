// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var ControllableSignalStrategy = function(jid, type, stateChangeCallback) {
  this.jid = jid;
  this.type = type;
  this.stateChangeCallback_ = stateChangeCallback;
  this.state_ = null;
  this.onIncomingStanzaCallback = function() {};
  this.dispose = sinon.spy();
  this.connect = sinon.spy();
  this.sendMessage = sinon.spy();
  this.sendConnectionSetupResults = sinon.spy();
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

ControllableSignalStrategy.prototype.getType = function(message) {
  return this.type;
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

var MockLogToServer = function() {
  this.logSignalStrategyProgress = sinon.spy();
};

MockLogToServer.prototype.assertProgress = function() {
  equal(this.logSignalStrategyProgress.callCount * 2, arguments.length);
  for (var i = 0; i < this.logSignalStrategyProgress.callCount; ++i) {
    equal(this.logSignalStrategyProgress.getCall(i).args[0], arguments[2 * i]);
    equal(this.logSignalStrategyProgress.getCall(i).args[1],
          arguments[2 * i + 1]);
  }
};

var onStateChange = null;
var onIncomingStanzaCallback = null;
var strategy = null;
var primary = null;
var secondary = null;
var logToServer = null;

module('fallback_signal_strategy', {
  setup: function() {
    onStateChange = sinon.spy();
    onIncomingStanzaCallback = sinon.spy();
    strategy = new remoting.FallbackSignalStrategy(
        new ControllableSignalStrategy('primary-jid',
                                       remoting.SignalStrategy.Type.XMPP),
        new ControllableSignalStrategy('secondary-jid',
                                       remoting.SignalStrategy.Type.WCS));
    strategy.setStateChangedCallback(onStateChange);
    strategy.setIncomingStanzaCallback(onIncomingStanzaCallback);
    primary = strategy.primary_;
    secondary = strategy.secondary_;
    logToServer = new MockLogToServer();
    primary.setExternalCallbackForTesting(onStateChange);
    secondary.setExternalCallbackForTesting(onStateChange);
  },
  teardown: function() {
    onStateChange = null;
    onIncomingStanzaCallback = null;
    strategy = null;
    primary = null;
    secondary = null;
    logToServer = null;
  },
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

    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    equal(strategy.getJid(), 'primary-jid');

    strategy.sendConnectionSetupResults(logToServer);
    logToServer.assertProgress(
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.SUCCEEDED);

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

    ok(!secondary.connect.called);
    primary.setStateForTesting(remoting.SignalStrategy.State.FAILED, false);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));

    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, true);

    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    equal(strategy.getJid(), 'secondary-jid');

    strategy.sendConnectionSetupResults(logToServer);
    logToServer.assertProgress(
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.FAILED,
        remoting.SignalStrategy.Type.WCS,
        remoting.FallbackSignalStrategy.Progress.SUCCEEDED);

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
    ok(!secondary.connect.called);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, true);
    primary.setStateForTesting(remoting.SignalStrategy.State.FAILED, false);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING, false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.FAILED, true);
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
    this.clock.tick(1);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, true);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    strategy.sendConnectionSetupResults(logToServer);

    secondary.setStateForTesting(remoting.SignalStrategy.State.CLOSED, true);
    primary.setStateForTesting(remoting.SignalStrategy.State.FAILED, false);

    logToServer.assertProgress(
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.TIMED_OUT,
        remoting.SignalStrategy.Type.WCS,
        remoting.FallbackSignalStrategy.Progress.SUCCEEDED,
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.FAILED_LATE);
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
    this.clock.tick(1);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.FAILED, true);
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
    secondary.setStateForTesting(remoting.SignalStrategy.State.NOT_CONNECTED,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTING,
                                 false);
    secondary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, true);
    secondary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, true);
    strategy.sendConnectionSetupResults(logToServer);

    primary.setStateForTesting(remoting.SignalStrategy.State.HANDSHAKE, false);
    primary.setStateForTesting(remoting.SignalStrategy.State.CONNECTED, false);

    logToServer.assertProgress(
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.TIMED_OUT,
        remoting.SignalStrategy.Type.WCS,
        remoting.FallbackSignalStrategy.Progress.SUCCEEDED,
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.SUCCEEDED_LATE);
  }
);

})();
