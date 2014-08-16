// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var appLauncher = null;
var hangoutPort = null;
var webappPort = null;
var it2meService = null;

function createPort(name, senderId) {
  var port = new chromeMocks.runtime.Port();
  port.name = (senderId) ? name +'@' + senderId : name;
  port.postMessage = sinon.spy(port, 'postMessage');
  port.disconnect = sinon.spy(port, 'disconnect');

  return port;
}

function promiseResolveSynchronous(value) {
  return {
    then: function(callback) {
      callback(value);
    }
  };
}

module('It2MeService', {
  setup: function() {
    // App Launcher.
    appLauncher = {
      launch: function () {
        return promiseResolveSynchronous('tabId');
      },
      close: function () {}
    };
    // HangoutPort.
    hangoutPort = createPort('it2me.helper.hangout');
    it2meService = new remoting.It2MeService(appLauncher);
    it2meService.onConnectExternal_(hangoutPort);
    webappPort = createPort('it2me.helper.webapp', 'tabId');
  }
});

test('should establish a channel two way channel when the webapp connects',
  function() {
    // Hangout ---- connect ----> It2MeService.
    hangoutPort.onMessage.mock$fire({
      method: 'connect',
      accessCode: "123412341234"
    });

    // Webapp ---- connect ----> It2MeService.
    it2meService.onWebappConnect_(webappPort);

    // Webapp ---- sessionStateChanged ----> It2MeService.
    webappPort.onMessage.mock$fire({
      method: 'sessionStateChanged',
      state: remoting.ClientSession.State.CONNECTED
    });

    // verify that hangout can receive message events.
    sinon.assert.calledWith(hangoutPort.postMessage, {
      method: 'sessionStateChanged',
      state: remoting.ClientSession.State.CONNECTED
    });

    hangoutPort.onDisconnect.mock$fire();
    QUnit.equal(it2meService.helpers_.length, 0);
});

test('should handle multiple helper connections', function() {
    // Hangout ---- connect ----> It2MeService.
    hangoutPort.onMessage.mock$fire({
      method: 'connect',
      accessCode: "123412341234"
    });

    // Hangout2 ---- connect ----> It2MeService.
    var hangoutPort2 = createPort('it2me.helper.hangout');
    it2meService.onConnectExternal_(hangoutPort2);

    appLauncher.launch = function () {
      return promiseResolveSynchronous('tabId2');
    };

    hangoutPort2.onMessage.mock$fire({
      method: 'connect',
      accessCode: "123412341234"
    });

    it2meService.onWebappConnect_(webappPort);

    var webappPort2 = createPort('it2me.helper.webapp', 'tabId2');
    it2meService.onWebappConnect_(webappPort2);

    webappPort.onMessage.mock$fire({
      method: 'sessionStateChanged',
      state: remoting.ClientSession.State.CONNECTED
    });

    // verify that hangout can receive message events from webapp 1
    sinon.assert.calledWith(hangoutPort.postMessage, {
      method: 'sessionStateChanged',
      state: remoting.ClientSession.State.CONNECTED
    });

    webappPort2.onMessage.mock$fire({
      method: 'sessionStateChanged',
      state: remoting.ClientSession.State.CLOSED
    });

    // verify that hangout can receive message events from webapp 2.
    sinon.assert.calledWith(hangoutPort2.postMessage, {
      method: 'sessionStateChanged',
      state: remoting.ClientSession.State.CLOSED
    });
});

test('should reject unknown connection', function() {
  it2meService.onWebappConnect_(webappPort);
  sinon.assert.called(webappPort.disconnect);

  var randomPort = createPort('unsupported.port.name');
  it2meService.onConnectExternal_(randomPort);
  sinon.assert.called(randomPort.disconnect);
});

})();