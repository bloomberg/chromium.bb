// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var appLauncher = null;
var hangoutPort = null;
var webappPort = null;
var helperChannel = null;
var disconnectCallback = null;

module('It2MeHelperChannel', {
  setup: function() {
    // App Launcher.
    appLauncher = {
      launch: function () {
        return promiseResolveSynchronous('tabId');
      },
      close: function () {}
    };
    appLauncher.launch = sinon.spy(appLauncher, 'launch');
    appLauncher.close = sinon.spy(appLauncher, 'close');

    // HangoutPort.
    hangoutPort = new chromeMocks.runtime.Port();
    hangoutPort.postMessage = sinon.spy(hangoutPort, 'postMessage');
    hangoutPort.disconnect = sinon.spy(hangoutPort, 'disconnect');

    // WebappPort.
    webappPort = new chromeMocks.runtime.Port();
    webappPort.sender = {
      tab : {
        id : 'tabId'
      }
    };
    webappPort.postMessage = sinon.spy(webappPort, 'postMessage');
    webappPort.disconnect = sinon.spy(webappPort, 'disconnect');

    // disconnect callback
    disconnectCallback = sinon.spy();

    // HelperChannel.
    helperChannel = new remoting.It2MeHelperChannel(
        appLauncher, hangoutPort, disconnectCallback);
    helperChannel.init();
    hangoutPort.onMessage.mock$fire({
      method: remoting.It2MeHelperChannel.HangoutMessageTypes.CONNECT,
      accessCode: "123412341234"
    });
  },
});

function promiseResolveSynchronous(value) {
  return {
    then: function(callback) {
      callback('tabId');
    }
  };
}

test('onHangoutMessage_(|connect|) should launch the webapp',
    function() {
  sinon.assert.called(appLauncher.launch);
  QUnit.equal(helperChannel.instanceId(), 'tabId');
});

test('onWebappMessage() should forward messages to hangout', function() {
  // Execute.
  helperChannel.onWebappConnect(webappPort);
  webappPort.onMessage.mock$fire({
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.CONNECTING
  });
  webappPort.onMessage.mock$fire({
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.CONNECTED
  });

  // Verify events are forwarded.
  sinon.assert.calledWith(hangoutPort.postMessage, {
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.CONNECTING
  });

  sinon.assert.calledWith(hangoutPort.postMessage, {
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.CONNECTED
  });
});

test('should notify hangout when the webapp crashes', function() {
  // Execute.
  helperChannel.onWebappConnect(webappPort);
  webappPort.onDisconnect.mock$fire();

  // Verify events are forwarded.
  sinon.assert.calledWith(hangoutPort.postMessage, {
    method:'sessionStateChanged',
    state: remoting.ClientSession.State.FAILED
  });
  sinon.assert.called(hangoutPort.disconnect);
  sinon.assert.calledOnce(disconnectCallback);
});

test('should notify hangout when the session is ended', function() {
  // Execute.
  helperChannel.onWebappConnect(webappPort);
  webappPort.onMessage.mock$fire({
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.CLOSED
  });

  webappPort.onDisconnect.mock$fire();

  // Verify events are forwarded.
  sinon.assert.calledWith(hangoutPort.postMessage, {
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.CLOSED
  });
  sinon.assert.called(hangoutPort.disconnect);
  sinon.assert.calledOnce(disconnectCallback);
});

test('should notify hangout when the session has error', function() {
  helperChannel.onWebappConnect(webappPort);
  webappPort.onMessage.mock$fire({
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.FAILED
  });

  webappPort.onDisconnect.mock$fire();

  // Verify events are forwarded.
  sinon.assert.calledWith(hangoutPort.postMessage, {
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.FAILED
  });
  sinon.assert.called(hangoutPort.disconnect);
  sinon.assert.calledOnce(disconnectCallback);
});


test('onHangoutMessages_(disconnect) should close the webapp', function() {
  // Execute.
  helperChannel.onWebappConnect(webappPort);
  hangoutPort.onMessage.mock$fire({
    method: remoting.It2MeHelperChannel.HangoutMessageTypes.DISCONNECT
  });

  sinon.assert.calledOnce(appLauncher.close);

  // Webapp will respond by disconnecting the port
  webappPort.onDisconnect.mock$fire();

  // Verify events are forwarded.
  sinon.assert.calledWith(hangoutPort.postMessage, {
    method:'sessionStateChanged',
    state:remoting.ClientSession.State.CLOSED
  });
  sinon.assert.called(webappPort.disconnect);
  sinon.assert.called(hangoutPort.disconnect);
});

test('should close the webapp when hangout crashes', function() {
  // Execute.
  helperChannel.onWebappConnect(webappPort);
  hangoutPort.onDisconnect.mock$fire();

  sinon.assert.calledOnce(appLauncher.close);
  sinon.assert.calledOnce(disconnectCallback);

  sinon.assert.called(hangoutPort.disconnect);
  sinon.assert.called(webappPort.disconnect);
});

})();
