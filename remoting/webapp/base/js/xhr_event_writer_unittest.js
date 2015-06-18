// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @type {remoting.XhrEventWriter} */
var eventWriter = null;
/** @type {!chromeMocks.StorageArea} */
var mockStorage;
/** @type {sinon.TestStub} */
var isOnlineStub;

/**
 * Flush the writer and ensure no outgoing requests is made.
 *
 * @param {QUnit.Assert} assert
 * @param {remoting.XhrEventWriter} writer
 * @return {Promise}
 */
function flushAndEnsureNoRequests(assert, writer) {
  var requestReceived = false;
  remoting.MockXhr.setResponseFor(
    'POST', 'fake_url', function(/** remoting.MockXhr */ xhr) {
      xhr.setTextResponse(200, '');
      requestReceived = true;
    });
  return writer.flush().then(function(){
    assert.ok(!requestReceived);
  });
}

QUnit.module('XhrEventWriter', {
  beforeEach: function() {
    remoting.MockXhr.activate();
    mockStorage = new chromeMocks.StorageArea();
    eventWriter = new remoting.XhrEventWriter(
        'fake_url', mockStorage, 'fake-storage-key');
    isOnlineStub = sinon.stub(base, 'isOnline');
  },
  afterEach: function() {
    isOnlineStub.restore();
    remoting.MockXhr.restore();
  }
});

QUnit.test('loadPendingRequests() handles empty storage.', function(assert){
  return eventWriter.loadPendingRequests().then(function(){
    return flushAndEnsureNoRequests(assert, eventWriter);
  });
});

QUnit.test('loadPendingRequests() handles corrupted data.', function(assert){
  var storage = mockStorage.mock$getStorage();
  storage['fake-storage-key'] = 'corrupted_data';
  return eventWriter.loadPendingRequests().then(function(){
    return flushAndEnsureNoRequests(assert, eventWriter);
  });
});

QUnit.test('write() should post XHR to server.', function(assert){
  isOnlineStub.returns(true);
  var requestReceived = false;
  remoting.MockXhr.setResponseFor(
    'POST', 'fake_url', function(/** remoting.MockXhr */ xhr) {
      assert.deepEqual(xhr.params.jsonContent, {hello: 'world'});
      xhr.setTextResponse(200, '');
      requestReceived = true;
    });

  return eventWriter.write({ hello: 'world'}).then(function(){
    assert.ok(requestReceived);
  });
});

QUnit.test('flush() should retry requests if OFFLINE.', function(assert){
  var requestReceived = false;
  isOnlineStub.returns(false);

  return eventWriter.write({ hello: 'world'}).then(function(){
    assert.ok(false, 'Expect to fail.');
  }).catch(function() {
    isOnlineStub.returns(true);
    remoting.MockXhr.setResponseFor(
      'POST', 'fake_url', function(/** remoting.MockXhr */ xhr) {
        assert.deepEqual(xhr.params.jsonContent, {hello: 'world'});
        requestReceived = true;
        xhr.setTextResponse(200, '');
      });
    return eventWriter.flush();
  }).then(function() {
    assert.ok(requestReceived);
  });
});

QUnit.test('flush() should not retry on server error.', function(assert){
  isOnlineStub.returns(true);
  remoting.MockXhr.setResponseFor(
    'POST', 'fake_url', function(/** remoting.MockXhr */ xhr) {
    assert.deepEqual(xhr.params.jsonContent, {hello: 'world'});
    xhr.setTextResponse(500, '');
  });

  return eventWriter.write({ hello: 'world'}).then(function(){
    assert.ok(false, 'Expect to fail.');
  }).catch(function() {
    return flushAndEnsureNoRequests(assert, eventWriter);
  });
});

QUnit.test('writeToStorage() should save pending requests.', function(assert){
  var requestReceived = false;
  var newEventWriter = new remoting.XhrEventWriter(
      'fake_url', mockStorage, 'fake-storage-key');
  isOnlineStub.returns(false);

  return eventWriter.write({ hello: 'world'}).then(function(){
    assert.ok(false, 'Expect to fail.');
  }).catch(function(){
    return eventWriter.writeToStorage();
  }).then(function() {
    return newEventWriter.loadPendingRequests();
  }).then(function() {
    isOnlineStub.returns(true);
    remoting.MockXhr.setResponseFor(
      'POST', 'fake_url', function(/** remoting.MockXhr */ xhr) {
      assert.deepEqual(xhr.params.jsonContent, {hello: 'world'});
      requestReceived = true;
      xhr.setTextResponse(200, '');
    });
    return newEventWriter.flush();
  }).then(function(){
    assert.ok(requestReceived);
  });
});

})();
