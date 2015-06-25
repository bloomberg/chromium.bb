// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @type {remoting.TelemetryEventWriter.Service} */
var service;
/** @type {remoting.XhrEventWriter} */
var eventWriter = null;

QUnit.module('TelemetryEventWriter', {
  beforeEach: function() {
    remoting.MockXhr.activate();
    var ipc = base.Ipc.getInstance();
    eventWriter =
        new remoting.XhrEventWriter('URL', chrome.storage.local, 'fake-key');
    service = new remoting.TelemetryEventWriter.Service(ipc, eventWriter);
  },
  afterEach: function() {
    base.dispose(service);
    service = null;
    base.Ipc.deleteInstance();
    remoting.MockXhr.restore();
  }
});

QUnit.test('Client.write() should write request.', function(assert){
  var mockEventWriter = sinon.mock(eventWriter);
  return service.init().then(function(){
    mockEventWriter.expects('write').once().withArgs({id: '1'});
    return remoting.TelemetryEventWriter.Client.write({id: '1'});
  }).then(function(){
    mockEventWriter.verify();
  });
});

QUnit.test('should save log requests on suspension.', function(assert){
  var mockEventWriter = sinon.mock(eventWriter);
  mockEventWriter.expects('writeToStorage').once();
  return service.init().then(function(){
    var mockSuspendEvent =
        /** @type {chromeMocks.Event} */ (chrome.runtime.onSuspend);
    mockSuspendEvent.mock$fire();
  }).then(function() {
    mockEventWriter.verify();
  });
});

QUnit.test('should flush log requests when online.', function(assert) {
  var mockEventWriter = sinon.mock(eventWriter);
  mockEventWriter.expects('flush').once();
  return service.init().then(function() {
    mockEventWriter.verify();
    mockEventWriter.expects('flush').once();
    window.dispatchEvent(new CustomEvent('online'));
  }).then(function() {
    mockEventWriter.verify();
  });
});

})();
