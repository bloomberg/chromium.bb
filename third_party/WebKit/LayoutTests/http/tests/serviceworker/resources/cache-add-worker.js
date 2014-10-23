importScripts('worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

cache_test(function(cache) {
    return assert_promise_rejects(
      cache.add(),
      new TypeError(),
      'Cache.add should throw a TypeError when no arguments are given.');
  }, 'Cache.add called with no arguments');

cache_test(function(cache) {
    return cache.add('simple.txt')
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        });
  }, 'Cache.add called with relative URL specified as a string');

cache_test(function(cache) {
    return assert_promise_rejects(
      cache.add('javascript://this-is-not-http-mmkay'),
      'NetworkError',
      'Cache.add should throw a NetworkError for non-HTTP/HTTPS URLs.');
  }, 'Cache.add called with non-HTTP/HTTPS URL');

cache_test(function(cache) {
    var request = new Request('simple.txt', {body: 'Hello'});
    return cache.add(request)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        });
  }, 'Cache.add called with Request object');

cache_test(function(cache) {
    var request = new Request('simple.txt', {body: 'Hello'});
    return request.text()
      .then(function() {
          assert_true(request.bodyUsed);
        })
      .then(function() {
          return assert_promise_rejects(
            cache.add(request),
            new TypeError(),
            'Cache.add with a Request object with a used body should reject ' +
            'with a TypeError.');
        });
  }, 'Cache.add called with Request object with a used body');

cache_test(function(cache) {
    var request = new Request('simple.txt', {body: 'Hello'});
    return cache.add(request)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        })
      .then(function() {
          return assert_promise_rejects(
            cache.add(request),
            new TypeError(),
            'Cache.add should throw TypeError if same request is added twice.');
        });
  }, 'Cache.add called twice with the same Request object');

cache_test(function(cache) {
    return cache.add('this-does-not-exist-please-dont-create-it')
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        });
  }, 'Cache.add with request that results in a status of 404');

cache_test(function(cache) {
    return cache.add('fetch-status.php?status=500')
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        });
  }, 'Cache.add with request that results in a status of 500');

cache_test(function(cache) {
    return assert_promise_rejects(
      cache.addAll(),
      new TypeError(),
      'Cache.addAll with no arguments should throw TypeError.');
  }, 'Cache.addAll with no arguments');

cache_test(function(cache) {
    // Assumes the existence of simple.txt and blank.html in the same directory
    // as this test script.
    var urls = ['simple.txt', undefined, 'blank.html'];
    return assert_promise_rejects(
      cache.addAll(),
      new TypeError(),
      'Cache.addAll should throw TypeError for an undefined argument.');
  }, 'Cache.addAll with a mix of valid and undefined arguments');

cache_test(function(cache) {
    // Assumes the existence of simple.txt and blank.html in the same directory
    // as this test script.
    var urls = ['simple.txt', self.location.href, 'blank.html'];
    return cache.addAll(urls)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.addAll should resolve with undefined on success.');
        });
  }, 'Cache.addAll with string URL arguments');

cache_test(function(cache) {
    // Assumes the existence of simple.txt and blank.html in the same directory
    // as this test script.
    var urls = ['simple.txt', self.location.href, 'blank.html'];
    var requests = urls.map(function(url) {
        return new Request(url);
      });
    return cache.addAll(requests)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.addAll should resolve with undefined on ' +
                        'success.');
        });
  }, 'Cache.addAll with Request arguments');

cache_test(function(cache) {
    // Assumes that simple.txt and blank.html exist. The second
    // resource does not.
    var urls = ['simple.txt', 'this-resource-should-not-exist', 'blank.html'];
    var requests = urls.map(function(url) {
        return new Request(url);
      });
    return cache.addAll(requests)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.addAll should resolve with undefined on ' +
                        'success.');
        });
  }, 'Cache.addAll with a mix of succeeding and failing requests');

cache_test(function(cache) {
    var request = new Request('simple.txt');
    return assert_promise_rejects(
      cache.addAll([request, request]),
      new TypeError(),
      'Cache.addAll should throw TypeError if the same request is added ' +
      'twice.');
  }, 'Cache.addAll called with the same Request object specified twice');
