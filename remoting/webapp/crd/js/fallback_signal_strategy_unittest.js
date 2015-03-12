// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * TODO(garykac): Create interfaces for LogToServer and SignalStrategy.
 * @suppress {checkTypes|checkVars|reportUnknownTypes|visibility}
 */

(function() {

'use strict';

/** @constructor */
var MockLogToServer = function() {
  /** @type {(sinon.Spy|Function)} */
  this.logSignalStrategyProgress = sinon.spy();
};

/** @type {function(...)} */
MockLogToServer.prototype.assertProgress = function() {
  equal(this.logSignalStrategyProgress.callCount * 2, arguments.length);
  for (var i = 0; i < this.logSignalStrategyProgress.callCount; ++i) {
    equal(this.logSignalStrategyProgress.getCall(i).args[0], arguments[2 * i]);
    equal(this.logSignalStrategyProgress.getCall(i).args[1],
          arguments[2 * i + 1]);
  }
};

/** @type {(sinon.Spy|function(remoting.SignalStrategy.State))} */
var onStateChange = null;

/** @type {(sinon.Spy|function(Element):void)} */
var onIncomingStanzaCallback = null;

/** @type {remoting.FallbackSignalStrategy} */
var strategy = null;

/** @type {remoting.SignalStrategy} */
var primary = null;

/** @type {remoting.SignalStrategy} */
var secondary = null;

/** @type {MockLogToServer} */
var logToServer = null;

/**
 * @param {remoting.MockSignalStrategy} baseSignalStrategy
 * @param {remoting.SignalStrategy.State} state
 * @param {boolean} expectCallback
 */
function setState(baseSignalStrategy, state, expectCallback) {
  onStateChange.reset();
  baseSignalStrategy.setStateForTesting(state);

  if (expectCallback) {
    equal(onStateChange.callCount, 1);
    ok(onStateChange.calledWith(state));
    equal(strategy.getState(), state);
  } else {
    ok(!onStateChange.called);
  }
};

module('fallback_signal_strategy', {
  setup: function() {
    onStateChange = sinon.spy();
    onIncomingStanzaCallback = sinon.spy();
    strategy = new remoting.FallbackSignalStrategy(
        new remoting.MockSignalStrategy('primary-jid',
                                        remoting.SignalStrategy.Type.XMPP),
        new remoting.MockSignalStrategy('secondary-jid',
                                        remoting.SignalStrategy.Type.WCS));
    strategy.setStateChangedCallback(onStateChange);
    strategy.setIncomingStanzaCallback(onIncomingStanzaCallback);
    primary = strategy.primary_;
    secondary = strategy.secondary_;
    logToServer = new MockLogToServer();
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

    setState(primary, remoting.SignalStrategy.State.NOT_CONNECTED, true);
    setState(primary, remoting.SignalStrategy.State.CONNECTING, true);
    setState(primary, remoting.SignalStrategy.State.HANDSHAKE, true);

    setState(primary, remoting.SignalStrategy.State.CONNECTED, true);
    equal(strategy.getJid(), 'primary-jid');

    strategy.sendConnectionSetupResults(logToServer);
    logToServer.assertProgress(
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.SUCCEEDED);

    ok(!onIncomingStanzaCallback.called);
    primary.onIncomingStanzaCallback_('test-receive-primary');
    secondary.onIncomingStanzaCallback_('test-receive-secondary');
    ok(onIncomingStanzaCallback.calledOnce);
    ok(onIncomingStanzaCallback.calledWith('test-receive-primary'));

    ok(!primary.sendMessage.called);
    strategy.sendMessage('test-send');
    ok(primary.sendMessage.calledOnce);
    ok(primary.sendMessage.calledWith('test-send'));

    ok(!primary.dispose.called);
    ok(!secondary.dispose.called);
    setState(primary, remoting.SignalStrategy.State.CLOSED, true);
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

    setState(primary, remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    setState(primary, remoting.SignalStrategy.State.CONNECTING, true);

    ok(!secondary.connect.called);
    setState(primary, remoting.SignalStrategy.State.FAILED, false);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));

    setState(secondary, remoting.SignalStrategy.State.NOT_CONNECTED, false);
    setState(secondary, remoting.SignalStrategy.State.CONNECTING, false);
    setState(secondary, remoting.SignalStrategy.State.HANDSHAKE, true);

    setState(secondary, remoting.SignalStrategy.State.CONNECTED, true);
    equal(strategy.getJid(), 'secondary-jid');

    strategy.sendConnectionSetupResults(logToServer);
    logToServer.assertProgress(
        remoting.SignalStrategy.Type.XMPP,
        remoting.FallbackSignalStrategy.Progress.FAILED,
        remoting.SignalStrategy.Type.WCS,
        remoting.FallbackSignalStrategy.Progress.SUCCEEDED);

    ok(!onIncomingStanzaCallback.called);
    primary.onIncomingStanzaCallback_('test-receive-primary');
    secondary.onIncomingStanzaCallback_('test-receive-secondary');
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

    setState(primary, remoting.SignalStrategy.State.NOT_CONNECTED, true);
    ok(!secondary.connect.called);
    setState(primary, remoting.SignalStrategy.State.CONNECTING, true);
    setState(primary, remoting.SignalStrategy.State.FAILED, false);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    setState(secondary, remoting.SignalStrategy.State.NOT_CONNECTED, false);
    setState(primary, remoting.SignalStrategy.State.CONNECTING, false);
    setState(secondary, remoting.SignalStrategy.State.FAILED, true);
  }
);

test('primary times out; secondary succeeds',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    setState(primary, remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    setState(primary, remoting.SignalStrategy.State.CONNECTING, true);
    this.clock.tick(strategy.PRIMARY_CONNECT_TIMEOUT_MS_ - 1);
    ok(!secondary.connect.called);
    this.clock.tick(1);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    setState(secondary, remoting.SignalStrategy.State.NOT_CONNECTED, false);
    setState(secondary, remoting.SignalStrategy.State.CONNECTING, false);
    setState(secondary, remoting.SignalStrategy.State.HANDSHAKE, true);
    setState(secondary, remoting.SignalStrategy.State.CONNECTED, true);
    strategy.sendConnectionSetupResults(logToServer);

    setState(secondary, remoting.SignalStrategy.State.CLOSED, true);
    setState(primary, remoting.SignalStrategy.State.FAILED, false);

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

    setState(primary, remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    setState(primary, remoting.SignalStrategy.State.CONNECTING, true);
    this.clock.tick(strategy.PRIMARY_CONNECT_TIMEOUT_MS_ - 1);
    ok(!secondary.connect.called);
    this.clock.tick(1);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    setState(secondary, remoting.SignalStrategy.State.NOT_CONNECTED, false);
    setState(secondary, remoting.SignalStrategy.State.CONNECTING, false);
    setState(secondary, remoting.SignalStrategy.State.FAILED, true);
  }
);

test('primary times out; secondary succeeds; primary succeeds late',
  function() {
    ok(!onStateChange.called);
    ok(!primary.connect.called);
    strategy.connect('server', 'username', 'authToken');
    ok(primary.connect.calledWith('server', 'username', 'authToken'));

    setState(primary, remoting.SignalStrategy.State.NOT_CONNECTED,
                               true);
    setState(primary, remoting.SignalStrategy.State.CONNECTING, true);
    this.clock.tick(strategy.PRIMARY_CONNECT_TIMEOUT_MS_);
    ok(secondary.connect.calledWith('server', 'username', 'authToken'));
    setState(secondary, remoting.SignalStrategy.State.NOT_CONNECTED, false);
    setState(secondary, remoting.SignalStrategy.State.CONNECTING, false);
    setState(secondary, remoting.SignalStrategy.State.HANDSHAKE, true);
    setState(secondary, remoting.SignalStrategy.State.CONNECTED, true);
    strategy.sendConnectionSetupResults(logToServer);

    setState(primary, remoting.SignalStrategy.State.HANDSHAKE, false);
    setState(primary, remoting.SignalStrategy.State.CONNECTED, false);

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
