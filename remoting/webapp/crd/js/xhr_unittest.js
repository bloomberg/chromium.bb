// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 */

(function() {

'use strict';

/** @type {sinon.FakeXhr} */
var fakeXhr;

QUnit.module('xhr', {
  beforeEach: function() {
    fakeXhr = null;
    sinon.useFakeXMLHttpRequest().onCreate =
        function(/** sinon.FakeXhr */ xhr) {
          fakeXhr = xhr;
        };
  }
});

QUnit.test('urlencodeParamHash', function(assert) {
  assert.equal(
      remoting.Xhr.urlencodeParamHash({}),
      '');
  assert.equal(
      remoting.Xhr.urlencodeParamHash({'key': 'value'}),
      'key=value');
  assert.equal(
      remoting.Xhr.urlencodeParamHash({'key /?=&': 'value /?=&'}),
      'key%20%2F%3F%3D%26=value%20%2F%3F%3D%26');
  assert.equal(
      remoting.Xhr.urlencodeParamHash({'k1': 'v1', 'k2': 'v2'}),
      'k1=v1&k2=v2');
});

QUnit.test('basic GET', function(assert) {
  var promise = new remoting.Xhr({
    method: 'GET',
    url: 'http://foo.com',
    responseType: remoting.Xhr.ResponseType.TEXT
  }).start().then(function(response) {
      assert.equal(response.status, 200);
      assert.equal(response.getText(), 'body');
  });
  assert.equal(fakeXhr.method, 'GET');
  assert.equal(fakeXhr.url, 'http://foo.com');
  assert.equal(fakeXhr.withCredentials, false);
  assert.equal(fakeXhr.requestBody, null);
  assert.ok(!('Content-type' in fakeXhr.requestHeaders));
  fakeXhr.respond(200, {}, 'body');
  return promise;
});

QUnit.test('GET with param string', function(assert) {
  var promise = new remoting.Xhr({
    method: 'GET',
    url: 'http://foo.com',
    responseType: remoting.Xhr.ResponseType.TEXT,
    urlParams: 'the_param_string'
  }).start().then(function(response) {
    assert.equal(response.status, 200);
    assert.equal(response.getText(), 'body');
  });
  assert.equal(fakeXhr.method, 'GET');
  assert.equal(fakeXhr.url, 'http://foo.com?the_param_string');
  assert.equal(fakeXhr.withCredentials, false);
  assert.equal(fakeXhr.requestBody, null);
  assert.ok(!('Content-type' in fakeXhr.requestHeaders));
  fakeXhr.respond(200, {}, 'body');
  return promise;
});

QUnit.test('GET with param object', function(assert) {
  var promise = new remoting.Xhr({
    method: 'GET',
    url: 'http://foo.com',
    responseType: remoting.Xhr.ResponseType.TEXT,
    urlParams: {'a': 'b', 'c': 'd'}
  }).start().then(function(response) {
    assert.equal(response.status, 200);
    assert.equal(response.getText(), 'body');
  });
  assert.equal(fakeXhr.method, 'GET');
  assert.equal(fakeXhr.url, 'http://foo.com?a=b&c=d');
  assert.equal(fakeXhr.withCredentials, false);
  assert.equal(fakeXhr.requestBody, null);
  assert.ok(!('Content-type' in fakeXhr.requestHeaders));
  fakeXhr.respond(200, {}, 'body');
  return promise;
});

QUnit.test('GET with headers', function(assert) {
  var promise = new remoting.Xhr({
    method: 'GET',
    url: 'http://foo.com',
    responseType: remoting.Xhr.ResponseType.TEXT,
    headers: {'Header1': 'headerValue1', 'Header2': 'headerValue2'}
  }).start().then(function(response) {
    assert.equal(response.status, 200);
    assert.equal(response.getText(), 'body');
  });
  assert.equal(fakeXhr.method, 'GET');
  assert.equal(fakeXhr.url, 'http://foo.com');
  assert.equal(fakeXhr.withCredentials, false);
  assert.equal(fakeXhr.requestBody, null);
  assert.equal(
      fakeXhr.requestHeaders['Header1'],
      'headerValue1');
  assert.equal(
      fakeXhr.requestHeaders['Header2'],
      'headerValue2');
  assert.ok(!('Content-type' in fakeXhr.requestHeaders));
  fakeXhr.respond(200, {}, 'body');
  return promise;
});


QUnit.test('GET with credentials', function(assert) {
  var promise = new remoting.Xhr({
    method: 'GET',
    url: 'http://foo.com',
    responseType: remoting.Xhr.ResponseType.TEXT,
    withCredentials: true
  }).start().then(function(response) {
    assert.equal(response.status, 200);
    assert.equal(response.getText(), 'body');
  });
  assert.equal(fakeXhr.method, 'GET');
  assert.equal(fakeXhr.url, 'http://foo.com');
  assert.equal(fakeXhr.withCredentials, true);
  assert.equal(fakeXhr.requestBody, null);
  assert.ok(!('Content-type' in fakeXhr.requestHeaders));
  fakeXhr.respond(200, {}, 'body');
  return promise;
});

QUnit.test('POST with text content', function(assert) {
  var done = assert.async();

  var promise = new remoting.Xhr({
    method: 'POST',
    url: 'http://foo.com',
    responseType: remoting.Xhr.ResponseType.TEXT,
    textContent: 'the_content_string'
  }).start().then(function(response) {
    assert.equal(response.status, 200);
    assert.equal(response.getText(), 'body');
    done();
  });
  assert.equal(fakeXhr.method, 'POST');
  assert.equal(fakeXhr.url, 'http://foo.com');
  assert.equal(fakeXhr.withCredentials, false);
  assert.equal(fakeXhr.requestBody, 'the_content_string');
  assert.ok(!('Content-type' in fakeXhr.requestHeaders));
  fakeXhr.respond(200, {}, 'body');
  return promise;
});

QUnit.test('POST with form content', function(assert) {
  var promise = new remoting.Xhr({
    method: 'POST',
    url: 'http://foo.com',
    responseType: remoting.Xhr.ResponseType.TEXT,
    formContent: {'a': 'b', 'c': 'd'}
  }).start().then(function(response) {
    assert.equal(response.status, 200);
    assert.equal(response.getText(), 'body');
  });
  assert.equal(fakeXhr.method, 'POST');
  assert.equal(fakeXhr.url, 'http://foo.com');
  assert.equal(fakeXhr.withCredentials, false);
  assert.equal(fakeXhr.requestBody, 'a=b&c=d');
  assert.equal(
      fakeXhr.requestHeaders['Content-type'],
      'application/x-www-form-urlencoded');
  fakeXhr.respond(200, {}, 'body');
  return promise;
});

QUnit.test('defaultResponse 200', function(assert) {
  var done = assert.async();

  var onDone = function() {
    assert.ok(true);
    done();
  };

  var onError = function(error) {
    assert.ok(false);
    done();
  };

  new remoting.Xhr({
    method: 'POST',
    url: 'http://foo.com'
  }).start().then(remoting.Xhr.defaultResponse(onDone, onError));
  fakeXhr.respond(200, {}, '');
});


QUnit.test('defaultResponse 404', function(assert) {
  var done = assert.async();

  var onDone = function() {
    assert.ok(false);
    done();
  };

  var onError = function(error) {
    assert.ok(true);
    done();
  };

  new remoting.Xhr({
    method: 'POST',
    url: 'http://foo.com'
  }).start().then(remoting.Xhr.defaultResponse(onDone, onError));
  fakeXhr.respond(404, {}, '');
});

})();
