importScripts('worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

var test_url = 'https://example.com/foo';

// Construct a generic Request object. The URL is |test_url|. All other fields
// are defaults.
function new_test_request() {
  return new Request(test_url);
}

// Construct a generic Response object. The URL is empty. If specified |body|
// will be set as the response body string.
function new_test_response(body) {
  body = body || 'Hello world!';
  return new Response(body, {
      status: 200,
      statusText: 'OK',
      headers: [['Content-Type', 'text/plain']]
    });
}

cache_test(function(cache) {
    var request = new_test_request();
    var response = new_test_response();
    return cache.put(request, response)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.put should resolve with undefined on success.');
        });
  }, 'Cache.put called with simple Request and Response');

cache_test(function(cache) {
    var test_url = new URL('simple.txt', location.href).href;
    var request = new Request(test_url);
    var response;
    return fetch(test_url)
        .then(function(fetch_result) {
          response = fetch_result.clone();
          return cache.put(request, fetch_result);
        })
      .then(function() {
          return cache.match(test_url);
        })
      .then(function(result) {
          assert_object_equals(result, response,
                               'Cache.put should update the cache with ' +
                               'new request and response.');
          return result.text();
        })
      .then(function(body) {
          assert_equals(body, 'a simple text file\n',
                        'Cache.put should store response body.');
        });
  }, 'Cache.put called with Request and Response from fetch()');

cache_test(function(cache) {
    var request = new Request(test_url, {
        method: 'GET',
        body: 'Hello'
      });
    var response = new_test_response();
    assert_false(request.bodyUsed,
                 '[https://fetch.spec.whatwg.org/#dom-body-bodyused] ' +
                 'Request.bodyUsed should be initially false.');
    return cache.put(request, response)
      .then(function() {
          assert_false(request.bodyUsed,
                       'Cache.put should not consume Request body.');
        })
      .then(function() {
          return cache.match(request);
        })
      .then(function(result) {
          assert_object_equals(result, response,
                               'Cache.put should store response body.');
        });
  }, 'Cache.put with Request containing a body');

cache_test(function(cache) {
    var request = new_test_request();
    var response = new_test_response();
    return cache.put(request, response)
      .then(function() {
          return cache.match(test_url);
        })
      .then(function(result) {
          assert_object_equals(result, response,
                               'Cache.put should update the cache with ' +
                               'new Request and Response.');
        });
  }, 'Cache.put with a Response containing an empty URL');

cache_test(function(cache) {
    var request = new_test_request();
    var response = new Response('', {
        status: 200,
        headers: [['Content-Type', 'text/plain']]
      });
    return cache.put(request, response)
      .then(function() {
          return cache.match(test_url);
        })
      .then(function(result) {
          assert_equals(result.status, 200, 'Cache.put should store status.');
          assert_equals(result.headers.get('Content-Type'), 'text/plain',
                        'Cache.put should store headers.');
          return result.text();
        })
      .then(function(body) {
          assert_equals(body, '',
                        'Cache.put should store response body.');
        });
  }, 'Cache.put with an empty response body');

cache_test(function(cache) {
    var test_url = new URL('fetch-status.php?status=500', location.href).href;
    var request = new Request(test_url);
    var response;
    return fetch(test_url)
      .then(function(fetch_result) {
          response = fetch_result.clone();
          return cache.put(request, fetch_result);
        })
      .then(function() {
          return cache.match(test_url);
        })
      .then(function(result) {
          assert_object_equals(result, response,
                               'Cache.put should update the cache with ' +
                               'new request and response.');
          return result.text();
        })
      .then(function(body) {
          assert_equals(body, '',
                        'Cache.put should store response body.');
        });
  }, 'Cache.put with HTTP 500 response');

cache_test(function(cache) {
  var alternate_response = new_test_response('Lorem ipsum');
    return cache.put(new_test_request(), new_test_response())
      .then(function() {
          return cache.put(new_test_request(), alternate_response);
        })
      .then(function() {
          return cache.match(test_url);
        })
      .then(function(result) {
          assert_object_equals(result, alternate_response,
                               'Cache.put should replace existing ' +
                               'response with new response.');
        });
  }, 'Cache.put called twice with same Request and different Responses');

cache_test(function(cache) {
    var url = 'http://example.com/foo';
    return cache.put(url, new_test_response('some body'))
      .then(function() { return cache.match(url); })
      .then(function(response) { return response.text(); })
      .then(function(body) {
          assert_equals(body, 'some body',
                        'Cache.put should accept a string as request.');
      });
  }, 'Cache.put with an string request');

cache_test(function(cache) {
    return assert_promise_rejects(
      cache.put(new_test_request(), 'Hello world!'),
      new TypeError(),
      'Cache.put should only accept a Response object as the response.');
  }, 'Cache.put with an invalid response');

cache_test(function(cache) {
  return assert_promise_rejects(
      cache.put(new Request('file:///etc/passwd'), new_test_response()),
      new TypeError(),
      'Cache.put should reject non-HTTP/HTTPS requests with a TypeError.');
  }, 'Cache.put with a non-HTTP/HTTPS request');

cache_test(function(cache) {
  var response = new_test_response();
    return cache.put(new Request('relative-url'), response)
      .then(function() {
          return cache.match(new URL('relative-url', location.href).href);
        })
      .then(function(result) {
          assert_object_equals(result, response,
                               'Cache.put should accept a relative URL ' +
                               'as the request.');
        });
  }, 'Cache.put with a relative URL');

cache_test(function(cache) {
    var request = new Request('http://example.com/foo', {method: 'HEAD'});
    return assert_promise_rejects(
      cache.put(request, new_test_response()),
      new TypeError(),
      'Cache.put should throw a TypeError for non-GET requests.');
  }, 'Cache.put with a non-GET request');

cache_test(function(cache) {
    return assert_promise_rejects(
      cache.put(new_test_request(), null),
      new TypeError(),
      'Cache.put should throw a TypeError for an empty response.');
  }, 'Cache.put with an empty response');
