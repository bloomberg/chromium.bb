// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/** @type {remoting.Me2MeTestDriver} */
var testDriver;

QUnit.module('Me2Me Telemetry Integration', {
  // We rely on our telemetry data to gain insight into the reliability and
  // the durability on our connections.  This test verifies the integrity of our
  // telemetry by ensuring that certain well known connection sequences will
  // generate the correct sequence of telemetry data.
  beforeEach: function() {
    testDriver = new remoting.Me2MeTestDriver();
  },
  afterEach: function() {
    base.dispose(testDriver);
    testDriver = null;
  }
});

/**
 * @param {remoting.Me2MeTestDriver} testDriver
 * @param {Object} baseEvent
 */
var expectSucceeded = function(testDriver, baseEvent) {
  var ChromotingEvent = remoting.ChromotingEvent;
  var sequence = [{
    session_state: ChromotingEvent.SessionState.STARTED,
  },{
    session_state: ChromotingEvent.SessionState.SIGNALING,
    previous_session_state: ChromotingEvent.SessionState.STARTED
  },{
    session_state: ChromotingEvent.SessionState.CREATING_PLUGIN,
    previous_session_state: ChromotingEvent.SessionState.SIGNALING
  },{
    session_state: ChromotingEvent.SessionState.CONNECTING,
    previous_session_state: ChromotingEvent.SessionState.CREATING_PLUGIN
  },{
    session_state: ChromotingEvent.SessionState.AUTHENTICATED,
    previous_session_state: ChromotingEvent.SessionState.CONNECTING
  },{
    session_state: ChromotingEvent.SessionState.CONNECTED,
    previous_session_state: ChromotingEvent.SessionState.AUTHENTICATED
  },{
    session_state: ChromotingEvent.SessionState.CLOSED,
    previous_session_state: ChromotingEvent.SessionState.CONNECTED
  }];

  var expectedEvents = sequence.map(function(/** Object */ sequenceValue) {
    var event = /** @type {Object} */ (base.deepCopy(baseEvent));
    base.mix(event, sequenceValue);
    return event;
  });
  testDriver.expectEvents(expectedEvents);
};

/**
 * @param {remoting.Me2MeTestDriver} testDriver
 * @param {Object} baseEvent
 */
var expectCanceled = function(testDriver, baseEvent) {
  var ChromotingEvent = remoting.ChromotingEvent;
  var sequence = [{
    session_state: ChromotingEvent.SessionState.STARTED,
  },{
    session_state: ChromotingEvent.SessionState.SIGNALING,
    previous_session_state: ChromotingEvent.SessionState.STARTED
  },{
    session_state: ChromotingEvent.SessionState.CREATING_PLUGIN,
    previous_session_state: ChromotingEvent.SessionState.SIGNALING
  },{
    session_state: ChromotingEvent.SessionState.CONNECTING,
    previous_session_state: ChromotingEvent.SessionState.CREATING_PLUGIN
  },{
    session_state: ChromotingEvent.SessionState.CONNECTION_CANCELED,
    previous_session_state: ChromotingEvent.SessionState.CONNECTING
  }];

  var expectedEvents = sequence.map(function(/** Object */ sequenceValue) {
    var event = /** @type {Object} */ (base.deepCopy(baseEvent));
    base.mix(event, sequenceValue);
    return event;
  });
  testDriver.expectEvents(expectedEvents);
};

/**
 * @param {remoting.Me2MeTestDriver} testDriver
 * @param {Object} baseEvent
 * @param {remoting.ChromotingEvent.ConnectionError} error
 */
var expectFailed = function(testDriver, baseEvent, error) {
  var ChromotingEvent = remoting.ChromotingEvent;
  var sequence = [{
    session_state: ChromotingEvent.SessionState.STARTED,
  },{
    session_state: ChromotingEvent.SessionState.SIGNALING,
    previous_session_state: ChromotingEvent.SessionState.STARTED
  },{
    session_state: ChromotingEvent.SessionState.CREATING_PLUGIN,
    previous_session_state: ChromotingEvent.SessionState.SIGNALING
  },{
    session_state: ChromotingEvent.SessionState.CONNECTING,
    previous_session_state: ChromotingEvent.SessionState.CREATING_PLUGIN
  },{
    session_state: ChromotingEvent.SessionState.CONNECTION_FAILED,
    previous_session_state: ChromotingEvent.SessionState.CONNECTING,
    connection_error: error
  }];

  var expectedEvents = sequence.map(function(/** Object */ sequenceValue) {
    var event = /** @type {Object} */ (base.deepCopy(baseEvent));
    base.mix(event, sequenceValue);
    return event;
  });
  testDriver.expectEvents(expectedEvents);
};

QUnit.test('Connection succeeded', function() {
  expectSucceeded(testDriver, {
    session_entry_point:
        remoting.ChromotingEvent.SessionEntryPoint.CONNECT_BUTTON,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  });

  /**
   * @param {remoting.MockClientPlugin} plugin
   * @param {remoting.ClientSession.State} state
   */
  function onStatusChanged(plugin, state) {
    if (state == remoting.ClientSession.State.CONNECTED) {
      testDriver.me2meActivity().stop();
      testDriver.endTest();
    }
  }

  testDriver.mockConnection().pluginFactory().mock$setPluginStatusChanged(
      onStatusChanged);

  return testDriver.startTest();
});

QUnit.test('Connection canceled - Pin prompt', function() {
   expectCanceled(testDriver, {
     session_entry_point:
         remoting.ChromotingEvent.SessionEntryPoint.CONNECT_BUTTON,
     role: remoting.ChromotingEvent.Role.CLIENT,
     mode: remoting.ChromotingEvent.Mode.ME2ME,
   });

  /**
   * @param {remoting.MockClientPlugin} plugin
   * @param {remoting.ClientSession.State} state
   */
  function onStatusChanged(plugin, state) {
    if (state == remoting.ClientSession.State.CONNECTING) {
      testDriver.cancelWhenPinPrompted();
      plugin.mock$onDisposed().then(function(){
        testDriver.endTest();
      });
    }
  }

  testDriver.mockConnection().pluginFactory().mock$setPluginStatusChanged(
      onStatusChanged);

  return testDriver.startTest();
});

QUnit.test('Connection failed - Signal strategy', function() {
  var EntryPoint = remoting.ChromotingEvent.SessionEntryPoint;
  testDriver.expectEvents([{
    session_entry_point: EntryPoint.CONNECT_BUTTON,
    session_state: remoting.ChromotingEvent.SessionState.STARTED
  },{
    session_entry_point: EntryPoint.CONNECT_BUTTON,
    session_state: remoting.ChromotingEvent.SessionState.SIGNALING,
    previous_session_state: remoting.ChromotingEvent.SessionState.STARTED
  },{
    session_entry_point: EntryPoint.CONNECT_BUTTON,
    previous_session_state: remoting.ChromotingEvent.SessionState.SIGNALING,
    session_state: remoting.ChromotingEvent.SessionState.CONNECTION_FAILED,
    connection_error: remoting.ChromotingEvent.ConnectionError.UNEXPECTED
  }]);

  var promise = testDriver.startTest();

  // The message dialog is shown when the connection fails.
  testDriver.mockDialogFactory().messageDialog.show = function() {
    testDriver.endTest();
    return Promise.resolve(remoting.MessageDialog.Result.PRIMARY);
  };

  var signalStrategy = testDriver.mockConnection().signalStrategy();
  signalStrategy.connect = function() {
    Promise.resolve().then(function(){
      signalStrategy.setStateForTesting(remoting.SignalStrategy.State.FAILED);
    });
  };

  return promise;
});

QUnit.test('Reconnect', function() {
  var EntryPoint = remoting.ChromotingEvent.SessionEntryPoint;
  expectSucceeded(testDriver, {
    session_entry_point: EntryPoint.CONNECT_BUTTON,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  });
  expectSucceeded(testDriver, {
    session_entry_point: EntryPoint.RECONNECT_BUTTON,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  });

  var count = 0;
  /**
   * @param {remoting.MockClientPlugin} plugin
   * @param {remoting.ClientSession.State} state
   */
  function onStatusChanged(plugin, state) {
    if (state == remoting.ClientSession.State.CONNECTED) {
      count++;
      if (count == 1) {
        testDriver.clickReconnectWhenFinished();
        testDriver.me2meActivity().stop();
      } else if (count == 2) {
        testDriver.clickOkWhenFinished();
        testDriver.me2meActivity().stop();
        testDriver.endTest();
      }
    }
  }

  testDriver.mockConnection().pluginFactory().mock$setPluginStatusChanged(
      onStatusChanged);

  return testDriver.startTest();
});

QUnit.test('HOST_OFFLINE - JID refresh failed', function() {
  var EntryPoint = remoting.ChromotingEvent.SessionEntryPoint;
  // Expects the first connection to fail with HOST_OFFLINE
  expectFailed(testDriver, {
    session_entry_point:EntryPoint.CONNECT_BUTTON,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  }, remoting.ChromotingEvent.ConnectionError.HOST_OFFLINE);

  function onPluginCreated(/** remoting.MockClientPlugin */ plugin) {
    plugin.mock$returnErrorOnConnect(
        remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE);
  }

  /**
   * @param {remoting.MockClientPlugin} plugin
   * @param {remoting.ClientSession.State} state
   */
  function onStatusChanged(plugin, state) {
    if (state == remoting.ClientSession.State.FAILED) {
      testDriver.endTest();
    }
  }

  testDriver.mockConnection().pluginFactory().mock$setPluginCreated(
      onPluginCreated);
  testDriver.mockConnection().pluginFactory().mock$setPluginStatusChanged(
      onStatusChanged);
  sinon.stub(testDriver.mockHostList(), 'refreshAndDisplay', function(callback){
      return Promise.reject();
  });

  return testDriver.startTest();
});

QUnit.test('HOST_OFFLINE - JID refresh succeeded', function() {
  var EntryPoint = remoting.ChromotingEvent.SessionEntryPoint;
  // Expects the first connection to fail with HOST_OFFLINE
  expectFailed(testDriver, {
    session_entry_point:EntryPoint.CONNECT_BUTTON,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  }, remoting.ChromotingEvent.ConnectionError.HOST_OFFLINE);
  // Expects the second connection to succeed with RECONNECT
  expectSucceeded(testDriver, {
    session_entry_point: EntryPoint.AUTO_RECONNECT_ON_HOST_OFFLINE,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  });

  var count = 0;
  function onPluginCreated(/** remoting.MockClientPlugin */ plugin) {
    count++;
    if (count == 1) {
      plugin.mock$returnErrorOnConnect(
          remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE);
    } else if (count == 2) {
      plugin.mock$useDefaultBehavior(
          remoting.MockClientPlugin.AuthMethod.PIN);
    }
  }

  /**
   * @param {remoting.MockClientPlugin} plugin
   * @param {remoting.ClientSession.State} state
   */
  function onStatusChanged(plugin, state) {
    if (state == remoting.ClientSession.State.CONNECTED) {
      testDriver.me2meActivity().stop();
      testDriver.endTest();
    }
  }

  testDriver.mockConnection().pluginFactory().mock$setPluginCreated(
      onPluginCreated);
  testDriver.mockConnection().pluginFactory().mock$setPluginStatusChanged(
      onStatusChanged);

  return testDriver.startTest();
});

QUnit.test('HOST_OFFLINE - Reconnect failed', function() {
  var EntryPoint = remoting.ChromotingEvent.SessionEntryPoint;
  // Expects the first connection to fail with HOST_OFFLINE
  expectFailed(testDriver, {
    session_entry_point:EntryPoint.CONNECT_BUTTON,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  }, remoting.ChromotingEvent.ConnectionError.HOST_OFFLINE);
  // Expects the second connection to fail with HOST_OVERLOAD
  expectFailed(testDriver, {
    session_entry_point:EntryPoint.AUTO_RECONNECT_ON_HOST_OFFLINE,
    role: remoting.ChromotingEvent.Role.CLIENT,
    mode: remoting.ChromotingEvent.Mode.ME2ME,
  }, remoting.ChromotingEvent.ConnectionError.HOST_OVERLOAD);

  var count = 0;
  function onPluginCreated(/** remoting.MockClientPlugin */ plugin) {
    count++;
    if (count == 1) {
      plugin.mock$returnErrorOnConnect(
          remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE);
    } else if (count == 2) {
      plugin.mock$returnErrorOnConnect(
          remoting.ClientSession.ConnectionError.HOST_OVERLOAD);
    }
  }

  var failureCount = 0;
  /**
   * @param {remoting.MockClientPlugin} plugin
   * @param {remoting.ClientSession.State} state
   */
  function onStatusChanged(plugin, state) {
    if (state == remoting.ClientSession.State.FAILED) {
      failureCount++;

      if (failureCount == 2) {
        testDriver.endTest();
      }
    }
  }
  testDriver.mockConnection().pluginFactory().mock$setPluginCreated(
      onPluginCreated);
  testDriver.mockConnection().pluginFactory().mock$setPluginStatusChanged(
      onStatusChanged);
  return testDriver.startTest();
});

})();
