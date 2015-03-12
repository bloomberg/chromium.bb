// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes|checkVars|reportUnknownTypes}
 */

(function() {

'use strict';

module('xhr', {
  setup: function() {
  },
  teardown: function() {
  }
});

test('urlencodeParamHash', function() {
  QUnit.equal(
      remoting.xhr.urlencodeParamHash({}),
      '');
  QUnit.equal(
      remoting.xhr.urlencodeParamHash({'key': 'value'}),
      'key=value');
  QUnit.equal(
      remoting.xhr.urlencodeParamHash({'key /?=&': 'value /?=&'}),
      'key%20%2F%3F%3D%26=value%20%2F%3F%3D%26');
  QUnit.equal(
      remoting.xhr.urlencodeParamHash({'k1': 'v1', 'k2': 'v2'}),
      'k1=v1&k2=v2');
});

asyncTest('basic GET', function() {
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      QUnit.ok(xhr === request);
      QUnit.equal(xhr.status, 200);
      QUnit.equal(xhr.responseText, 'body');
      QUnit.start();
    }
  });
  QUnit.equal(request.method, 'GET');
  QUnit.equal(request.url, 'http://foo.com');
  QUnit.equal(request.withCredentials, false);
  QUnit.equal(request.requestBody, null);
  QUnit.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

asyncTest('GET with param string', function() {
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      QUnit.ok(xhr === request);
      QUnit.equal(xhr.status, 200);
      QUnit.equal(xhr.responseText, 'body');
      QUnit.start();
    },
    urlParams: 'the_param_string'
  });
  QUnit.equal(request.method, 'GET');
  QUnit.equal(request.url, 'http://foo.com?the_param_string');
  QUnit.equal(request.withCredentials, false);
  QUnit.equal(request.requestBody, null);
  QUnit.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

asyncTest('GET with param object', function() {
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      QUnit.ok(xhr === request);
      QUnit.equal(xhr.status, 200);
      QUnit.equal(xhr.responseText, 'body');
      QUnit.start();
    },
    urlParams: {'a': 'b', 'c': 'd'}
  });
  QUnit.equal(request.method, 'GET');
  QUnit.equal(request.url, 'http://foo.com?a=b&c=d');
  QUnit.equal(request.withCredentials, false);
  QUnit.equal(request.requestBody, null);
  QUnit.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

asyncTest('GET with headers', function() {
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      QUnit.ok(xhr === request);
      QUnit.equal(xhr.status, 200);
      QUnit.equal(xhr.responseText, 'body');
      QUnit.start();
    },
    headers: {'Header1': 'headerValue1', 'Header2': 'headerValue2'}
  });
  QUnit.equal(request.method, 'GET');
  QUnit.equal(request.url, 'http://foo.com');
  QUnit.equal(request.withCredentials, false);
  QUnit.equal(request.requestBody, null);
  QUnit.equal(
      request.requestHeaders['Header1'],
      'headerValue1');
  QUnit.equal(
      request.requestHeaders['Header2'],
      'headerValue2');
  QUnit.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});


asyncTest('GET with credentials', function() {
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'GET',
    url: 'http://foo.com',
    onDone: function(xhr) {
      QUnit.ok(xhr === request);
      QUnit.equal(xhr.status, 200);
      QUnit.equal(xhr.responseText, 'body');
      QUnit.start();
    },
    withCredentials: true
  });
  QUnit.equal(request.method, 'GET');
  QUnit.equal(request.url, 'http://foo.com');
  QUnit.equal(request.withCredentials, true);
  QUnit.equal(request.requestBody, null);
  QUnit.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

asyncTest('POST with text content', function() {
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: function(xhr) {
      QUnit.ok(xhr === request);
      QUnit.equal(xhr.status, 200);
      QUnit.equal(xhr.responseText, 'body');
      QUnit.start();
    },
    textContent: 'the_content_string'
  });
  QUnit.equal(request.method, 'POST');
  QUnit.equal(request.url, 'http://foo.com');
  QUnit.equal(request.withCredentials, false);
  QUnit.equal(request.requestBody, 'the_content_string');
  QUnit.ok(!('Content-type' in request.requestHeaders));
  request.respond(200, {}, 'body');
});

asyncTest('POST with form content', function() {
  sinon.useFakeXMLHttpRequest();
  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: function(xhr) {
      QUnit.ok(xhr === request);
      QUnit.equal(xhr.status, 200);
      QUnit.equal(xhr.responseText, 'body');
      QUnit.start();
    },
    formContent: {'a': 'b', 'c': 'd'}
  });
  QUnit.equal(request.method, 'POST');
  QUnit.equal(request.url, 'http://foo.com');
  QUnit.equal(request.withCredentials, false);
  QUnit.equal(request.requestBody, 'a=b&c=d');
  QUnit.equal(
      request.requestHeaders['Content-type'],
      'application/x-www-form-urlencoded');
  request.respond(200, {}, 'body');
});

asyncTest('defaultResponse 200', function() {
  sinon.useFakeXMLHttpRequest();

  var onDone = function() {
    QUnit.ok(true);
    QUnit.start();
  };

  var onError = function(error) {
    QUnit.ok(false);
    QUnit.start();
  };

  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: remoting.xhr.defaultResponse(onDone, onError)
  });
  request.respond(200);
});


asyncTest('defaultResponse 404', function() {
  sinon.useFakeXMLHttpRequest();

  var onDone = function() {
    QUnit.ok(false);
    QUnit.start();
  };

  var onError = function(error) {
    QUnit.ok(true);
    QUnit.start();
  };

  var request = remoting.xhr.start({
    method: 'POST',
    url: 'http://foo.com',
    onDone: remoting.xhr.defaultResponse(onDone, onError)
  });
  request.respond(404);
});

})();
