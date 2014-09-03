// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var hostInstaller = null;
var hangoutPort = null;
var host = null;
var helpeeChannel = null;
var onDisposedCallback = null;

module('It2MeHelpeeChannel', {
  setup: function() {
    // HangoutPort
    hangoutPort = new chromeMocks.runtime.Port();
    hangoutPort.postMessage = sinon.spy(hangoutPort, 'postMessage');
    hangoutPort.disconnect = sinon.spy(hangoutPort, 'disconnect');

    // onDisposedCallback callback
    onDisposedCallback = sinon.spy();

    // Host
    host = {
      initialize: function() {},
      initialized: function() {},
      connect: function() {},
      disconnect: function() {},
      getAccessCode: function() {},
      unhookCallbacks: function() {}
    };

    // HostInstaller
    hostInstaller = {
      isInstalled: function() {},
      download: function() {}
    };

    // HelpeeChannel
    helpeeChannel = new remoting.It2MeHelpeeChannel(
        hangoutPort,
        host,
        hostInstaller,
        onDisposedCallback);
    helpeeChannel.init();

    // remoting.settings
    remoting.settings = new remoting.Settings();
  }
});

test('hello() should return supportedFeatures', function() {
  hangoutPort.onMessage.mock$fire(
      { method: remoting.It2MeHelpeeChannel.HangoutMessageTypes.HELLO });

  sinon.assert.calledWith(hangoutPort.postMessage, {
    method: remoting.It2MeHelpeeChannel.HangoutMessageTypes.HELLO_RESPONSE,
    supportedFeatures: base.values(remoting.It2MeHelperChannel.Features)
  });
});

QUnit.asyncTest(
    'isHostInstalled() should return false if host is not installed',
    function() {
  sinon.stub(hostInstaller, 'isInstalled').returns(Promise.resolve(false));

  var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;
  hangoutPort.onMessage.mock$fire({
    method: MessageTypes.IS_HOST_INSTALLED
  });

  window.requestAnimationFrame(function() {
    sinon.assert.calledWith(hangoutPort.postMessage, {
      method: MessageTypes.IS_HOST_INSTALLED_RESPONSE,
      result: false
    });
    QUnit.start();
  });
});

QUnit.asyncTest('isHostInstalled() should return true if host is installed',
    function() {
  sinon.stub(hostInstaller, 'isInstalled').returns(Promise.resolve(true));

  var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;
  hangoutPort.onMessage.mock$fire({
    method: MessageTypes.IS_HOST_INSTALLED
  });

  window.requestAnimationFrame(function() {
    sinon.assert.calledWith(hangoutPort.postMessage, {
      method: MessageTypes.IS_HOST_INSTALLED_RESPONSE,
      result: true
    });
    QUnit.start();
  });
});

test('downloadHost() should trigger a host download',
    function() {
  sinon.stub(hostInstaller, 'download').returns(Promise.resolve(true));

  hangoutPort.onMessage.mock$fire({
    method: remoting.It2MeHelpeeChannel.HangoutMessageTypes.DOWNLOAD_HOST
  });

  sinon.assert.called(hostInstaller.download);
});

test('connect() should return error if email is missing',
    function() {
  var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;

  hangoutPort.onMessage.mock$fire({
    method: MessageTypes.CONNECT
  });

  sinon.assert.calledWithMatch(hangoutPort.postMessage, {
      method: MessageTypes.ERROR
  });
});

QUnit.asyncTest('connect() should return access code',
    function() {
  // Stubs authentication.
  sinon.stub(base, 'isAppsV2').returns(true);
  sinon.stub(remoting.MessageWindow, 'showConfirmWindow')
      .callsArgWith(4, 1 /* 1 for OK. */);
  sinon.stub(chrome.identity, 'getAuthToken')
      .callsArgWith(1, 'token');
  // Stubs Host behavior.
  sinon.stub(host, 'initialized').returns(true);
  sinon.stub(host, 'connect')
      .callsArgWith(2, remoting.HostSession.State.RECEIVED_ACCESS_CODE);
  sinon.stub(host, 'getAccessCode').returns('accessCode');

  var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;
  hangoutPort.onMessage.mock$fire({
    method: MessageTypes.CONNECT,
    email: 'test@chromium.org'
  });

  window.requestAnimationFrame(function(){
    // Verify that access code is correct in the response.
    sinon.assert.calledWithMatch(hangoutPort.postMessage, {
        method: MessageTypes.CONNECT_RESPONSE,
        accessCode: 'accessCode'
    });

    chrome.identity.getAuthToken.restore();
    base.isAppsV2.restore();
    remoting.MessageWindow.showConfirmWindow.restore();
    QUnit.start();
  });
});

test('should disconnect the session if Hangout crashes', function() {
  sinon.spy(host, 'disconnect');
  hangoutPort.onDisconnect.mock$fire();

  sinon.assert.called(onDisposedCallback);
  sinon.assert.called(host.disconnect);
});

})();
