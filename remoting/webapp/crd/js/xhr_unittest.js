// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes|checkVars|reportUnknownTypes}
 */

(function() {

'use strict';
QUnit.module('xhr');

QUnit.test('urlencodeParamHash', function(assert) {
  assert.equal(
      remoting.xhr.urlencodeParamHash({}),
      '');
  assert.equal(
      remoting.xhr.urlencodeParamHash({'key': 'value'}),
      'key=value');
  assert.equal(
      remoting.xhr.urlencodeParamHash({'key /?=&': 'value /?=&'}),
      'key%20%2F%3F%3D%26=value%20%2F%3F%3D%26');
  assert.equal(
      remoting.xhr.urlencodeParamHash({'k1': 'v1', 'k2': 'v2'}),
      'k1=v1&k2=v2');
});

QUnit.test('basic GET', function(assert) {
  sinon.useFakeXMLHttpRequest();
  var done = assert.async();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      assert.ok(xhr === request);
      assert.equal(xhr.status, 200);
      assert.equal(xhr.responseText, 'body');
      done();
    }
  });
  assert.equal(request.method, 'GET');
  assert.equal(request.url, 'http://foo.com');
  assert.equal(request.withCredentials, false);
  assert.equal(request.requestBody, null);
  assert.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

QUnit.test('GET with param string', function(assert) {
  var done = assert.async();
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      assert.ok(xhr === request);
      assert.equal(xhr.status, 200);
      assert.equal(xhr.responseText, 'body');
      done();
    },
    urlParams: 'the_param_string'
  });
  assert.equal(request.method, 'GET');
  assert.equal(request.url, 'http://foo.com?the_param_string');
  assert.equal(request.withCredentials, false);
  assert.equal(request.requestBody, null);
  assert.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

QUnit.test('GET with param object', function(assert) {
  var done = assert.async();
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      assert.ok(xhr === request);
      assert.equal(xhr.status, 200);
      assert.equal(xhr.responseText, 'body');
      done();
    },
    urlParams: {'a': 'b', 'c': 'd'}
  });
  assert.equal(request.method, 'GET');
  assert.equal(request.url, 'http://foo.com?a=b&c=d');
  assert.equal(request.withCredentials, false);
  assert.equal(request.requestBody, null);
  assert.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

QUnit.test('GET with headers', function(assert) {
  sinon.useFakeXMLHttpRequest();
  var done = assert.async();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      assert.ok(xhr === request);
      assert.equal(xhr.status, 200);
      assert.equal(xhr.responseText, 'body');
      done();
    },
    headers: {'Header1': 'headerValue1', 'Header2': 'headerValue2'}
  });
  assert.equal(request.method, 'GET');
  assert.equal(request.url, 'http://foo.com');
  assert.equal(request.withCredentials, false);
  assert.equal(request.requestBody, null);
  assert.equal(
      request.requestHeaders['Header1'],
      'headerValue1');
  assert.equal(
      request.requestHeaders['Header2'],
      'headerValue2');
  assert.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});


QUnit.test('GET with credentials', function(assert) {
  sinon.useFakeXMLHttpRequest();
  var done = assert.async();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      assert.ok(xhr === request);
      assert.equal(xhr.status, 200);
      assert.equal(xhr.responseText, 'body');
      done();
    },
    withCredentials: true
  });
  assert.equal(request.method, 'GET');
  assert.equal(request.url, 'http://foo.com');
  assert.equal(request.withCredentials, true);
  assert.equal(request.requestBody, null);
  assert.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

QUnit.test('POST with text content', function(assert) {
  sinon.useFakeXMLHttpRequest();
  var done = assert.async();
  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: function(xhr) {
      assert.ok(xhr === request);
      assert.equal(xhr.status, 200);
      assert.equal(xhr.responseText, 'body');
      done();
    },
    textContent: 'the_content_string'
  });
  assert.equal(request.method, 'POST');
  assert.equal(request.url, 'http://foo.com');
  assert.equal(request.withCredentials, false);
  assert.equal(request.requestBody, 'the_content_string');
  assert.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

QUnit.test('POST with form content', function(assert) {
  sinon.useFakeXMLHttpRequest();
  var done = assert.async();
  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: function(xhr) {
      assert.ok(xhr === request);
      assert.equal(xhr.status, 200);
      assert.equal(xhr.responseText, 'body');
      done();
    },
    formContent: {'a': 'b', 'c': 'd'}
  });
  assert.equal(request.method, 'POST');
  assert.equal(request.url, 'http://foo.com');
  assert.equal(request.withCredentials, false);
  assert.equal(request.requestBody, 'a=b&c=d');
  assert.equal(
      request.requestHeaders['Content-type'],
      'application/x-www-form-urlencoded');
  request.respond(200, {}, 'body');
});

QUnit.test('defaultResponse 200', function(assert) {
  sinon.useFakeXMLHttpRequest();
  var done = assert.async();

  var onDone = function() {
    assert.ok(true);
    done();
  };

  var onError = function(error) {
    assert.ok(false);
    done();
  };

  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: remoting.xhr.defaultResponse(onDone, onError)
  });
  request.respond(200);
});


QUnit.test('defaultResponse 404', function(assert) {
  sinon.useFakeXMLHttpRequest();
  var done = assert.async();
  var onDone = function() {
    assert.ok(false);
    done();
  };

  var onError = function(error) {
    assert.ok(true);
    done();
  };

  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: remoting.xhr.defaultResponse(onDone, onError)
  });
  request.respond(404);
});

})();
