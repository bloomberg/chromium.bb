if (self.importScripts) {
    importScripts('/resources/testharness.js');
    importScripts('../resources/test-helpers.js');
}

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys('not-present-in-the-cache')
      .then(function(result) {
          assert_request_array_equals(
            result, [],
            'Cache.keys should resolve with an empty array on failure.');
        });
  }, 'Cache.keys with no matching entries');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a.request.url)
      .then(function(result) {
          assert_request_array_equals(result, [entries.a.request],
                                      'Cache.keys should match by URL.');
        });
  }, 'Cache.keys with URL');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a.request)
      .then(function(result) {
          assert_request_array_equals(
            result, [entries.a.request],
            'Cache.keys should match by Request.');
        });
  }, 'Cache.keys with Request');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(new Request(entries.a.request.url))
      .then(function(result) {
          assert_request_array_equals(
            result, [entries.a.request],
            'Cache.keys should match by Request.');
        });
  }, 'Cache.keys with new Request');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a.request, {ignoreSearch: true})
      .then(function(result) {
          // TODO(zino): Should use assert_request_array_equals() instead of
          // assert_request_array_equivalent() once keys() returns request
          // keys in key insertion order. Please see http://crbug.com/627821.
          assert_request_array_equivalent(
            result,
            [
              entries.a.request,
              entries.a_with_query.request
            ],
            'Cache.keys with ignoreSearch should ignore the ' +
            'search parameters of cached request.');
        });
  },
  'Cache.keys with ignoreSearch option (request with no search ' +
  'parameters)');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a_with_query.request, {ignoreSearch: true})
      .then(function(result) {
          // TODO(zino): Should use assert_request_array_equals() instead of
          // assert_request_array_equivalent() if once keys() returns request
          // keys in key insertion order. Please see http://crbug.com/627821.
          assert_request_array_equivalent(
            result,
            [
              entries.a.request,
              entries.a_with_query.request
            ],
            'Cache.keys with ignoreSearch should ignore the ' +
            'search parameters of request.');
        });
  },
  'Cache.keys with ignoreSearch option (request with search parameters)');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.cat.request.url + '#mouse')
      .then(function(result) {
          assert_request_array_equals(
            result,
            [
              entries.cat.request,
            ],
            'Cache.keys should ignore URL fragment.');
        });
  }, 'Cache.keys with URL containing fragment');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys('http')
      .then(function(result) {
          assert_request_array_equals(
            result, [],
            'Cache.keys should treat query as a URL and not ' +
            'just a string fragment.');
        });
  }, 'Cache.keys with string fragment "http" as query');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys()
      .then(function(result) {
          // TODO(zino): Should use assert_request_array_equals() instead of
          // assert_request_array_equivalent() once keys() returns request
          // keys in key insertion order. Please see http://crbug.com/627821.
          assert_request_array_equivalent(
            result,
            [
              entries.a.request,
              entries.b.request,
              entries.a_with_query.request,
              entries.A.request,
              entries.a_https.request,
              entries.a_org.request,
              entries.cat.request,
              entries.catmandu.request,
              entries.cat_num_lives.request,
              entries.cat_in_the_hat.request,
              entries.non_2xx_response.request,
              entries.error_response.request
            ],
            'Cache.keys without parameters should match all entries.');
        });
  }, 'Cache.keys without parameters');

done();
