importScripts('worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

// A set of Request/Response pairs to be used with prepopulated_cache_test().
var simple_entries = {
  a: {
    request: new Request('http://example.com/a'),
    response: new Response('')
  },

  b: {
    request: new Request('http://example.com/b'),
    response: new Response('')
  },

  a_with_query: {
    request: new Request('http://example.com/a?q=r'),
    response: new Response('')
  },

  A: {
    request: new Request('http://example.com/A'),
    response: new Response('')
  },

  a_https: {
    request: new Request('https://example.com/a'),
    response: new Response('')
  },

  a_org: {
    request: new Request('http://example.org/a'),
    response: new Response('')
  },

  cat: {
    request: new Request('http://example.com/cat'),
    response: new Response('')
  },

  cat_with_fragment: {
    request: new Request('http://example.com/cat#mouse'),
    response: new Response('')
  },

  cat_in_the_hat: {
    request: new Request('http://example.com/cat/in/the/hat'),
    response: new Response('')
  }
};

// A set of Request/Response pairs to be used with prepopulated_cache_test().
// These contain a mix of test cases that use Vary headers.
var vary_entries = {
  no_vary_header: {
    request: new Request('http://example.com/c'),
    response: new Response('')
  },

  vary_cookie_is_cookie: {
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'is-for-cookie'}}),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  vary_cookie_is_good: {
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'is-good-enough-for-me'}}),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  vary_cookie_absent: {
    request: new Request('http://example.com/c'),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  vary_wildcard: {
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'x', 'X-Key': '1'}}),
    response: new Response('',
                           {headers: {'Vary': '*'}})
  }
};

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll(simple_entries.a.request.url)
      .then(function(result) {
          assert_array_objects_equals(result, [simple_entries.a.response],
                                      'Cache.matchAll should match by URL.');
        });
  }, 'Cache.matchAll with URL');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.match(simple_entries.a.request.url)
      .then(function(result) {
          assert_object_equals(result, simple_entries.a.response,
                               'Cache.match should match by URL.');
        });
  }, 'Cache.match with URL');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll(simple_entries.a.request)
      .then(function(result) {
          assert_array_objects_equals(
            result, [simple_entries.a.response],
            'Cache.matchAll should match by Request.');
        });
  }, 'Cache.matchAll with Request');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.match(simple_entries.a.request)
      .then(function(result) {
          assert_object_equals(result, simple_entries.a.response,
                               'Cache.match should match by Request.');
        });
  }, 'Cache.match with Request');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll(new Request(simple_entries.a.request.url))
      .then(function(result) {
          assert_array_objects_equals(
            result, [simple_entries.a.response],
            'Cache.matchAll should match by Request.');
        });
  }, 'Cache.matchAll with new Request');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.match(new Request(simple_entries.a.request.url))
      .then(function(result) {
          assert_object_equals(result, simple_entries.a.response,
                               'Cache.match should match by Request.');
        });
  }, 'Cache.match with new Request');

cache_test(function(cache) {
    var request = new Request('https://example.com/foo', {
        method: 'GET',
        body: 'Hello world!'
      });
    var response = new Response('Booyah!', {
        status: 200,
        headers: {'Content-Type': 'text/plain'}
      });

    return cache.put(request.clone(), response.clone())
      .then(function() {
          assert_false(
            request.bodyUsed,
            '[https://fetch.spec.whatwg.org/#concept-body-used-flag] ' +
            'Request.bodyUsed flag should be initially false.');
        })
      .then(function() {
          return cache.match(request);
        })
      .then(function(result) {
          assert_false(request.bodyUsed,
                       'Cache.match should not consume Request body.');
        });
  }, 'Cache.match with Request containing non-empty body');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll(simple_entries.a.request,
                          {ignoreSearch: true})
      .then(function(result) {
          assert_array_equivalent(
            result,
            [
              simple_entries.a.response,
              simple_entries.a_with_query.response
            ],
            'Cache.matchAll with ignoreSearch should ignore the ' +
            'search parameters of cached request.');
        });
  },
  'Cache.matchAll with ignoreSearch option (request with no search ' +
  'parameters)');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll(simple_entries.a_with_query.request,
                          {ignoreSearch: true})
      .then(function(result) {
          assert_array_equivalent(
            result,
            [
              simple_entries.a.response,
              simple_entries.a_with_query.response
            ],
            'Cache.matchAll with ignoreSearch should ignore the ' +
            'search parameters of request.');
        });
  },
  'Cache.matchAll with ignoreSearch option (request with search parameter)');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll(simple_entries.cat.request)
      .then(function(result) {
          assert_array_equivalent(
            result,
            [
              simple_entries.cat.response,
              simple_entries.cat_with_fragment.response
            ],
            'Cache.matchAll should ignore URL hash.');
        });
  }, 'Cache.matchAll with request containing hash');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll('http')
      .then(function(result) {
          assert_array_equivalent(
            result, [],
            'Cache.matchAll should treat query as a URL and not ' +
            'just a string fragment.');
        });
  }, 'Cache.matchAll with string fragment "http" as query');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll('http://example.com/cat',
                          {prefixMatch: true})
      .then(function(result) {
          assert_array_equivalent(
            result,
            [
              simple_entries.cat.response,
              simple_entries.cat_with_fragment.response,
              simple_entries.cat_in_the_hat.response
            ],
            'Cache.matchAll should honor prefixMatch.');
        });
  }, 'Cache.matchAll with prefixMatch option');

prepopulated_cache_test(simple_entries, function(cache) {
    return cache.matchAll('http://example.com/cat/',
                          {prefixMatch: true})
      .then(function(result) {
          assert_array_equivalent(
            result, [simple_entries.cat_in_the_hat.response],
            'Cache.matchAll should honor prefixMatch.');
        });
  }, 'Cache.matchAll with prefixMatch option');

prepopulated_cache_test(vary_entries, function(cache) {
    return cache.matchAll('http://example.com/c')
      .then(function(result) {
          assert_array_equivalent(
            result,
            [
              vary_entries.no_vary_header.response,
              vary_entries.vary_wildcard.response,
              vary_entries.vary_cookie_absent.response
            ],
            'Cache.matchAll should exclude matches if a vary header is ' +
            'missing in the query request, but is present in the cached ' +
            'request.');
        })

      .then(function() {
          return cache.matchAll(
            new Request('http://example.com/c',
                        {headers: {'Cookies': 'none-of-the-above'}}));
        })
      .then(function(result) {
          assert_array_equivalent(
            result,
            [
              vary_entries.no_vary_header.response,
              vary_entries.vary_wildcard.response
            ],
            'Cache.matchAll should exclude matches if a vary header is ' +
            'missing in the cached request, but is present in the query ' +
            'request.');
        })

      .then(function() {
          return cache.matchAll(
            new Request('http://example.com/c',
                        {headers: {'Cookies': 'is-for-cookie'}}));
        })
      .then(function(result) {
          assert_array_equivalent(
            result,
            [vary_entries.vary_cookie_is_cookie.response],
            'Cache.matchAll should match the entire header if a vary header ' +
            'is present in both the query and cached requests.');
        });
  }, 'Cache.matchAll with responses containing "Vary" header');

prepopulated_cache_test(vary_entries, function(cache) {
    return cache.match('http://example.com/c')
      .then(function(result) {
          assert_object_in_array(
            result,
            [
              vary_entries.no_vary_header.response,
              vary_entries.vary_wildcard.response,
              vary_entries.vary_cookie_absent.response
            ],
            'Cache.match should honor "Vary" header.');
        });
  }, 'Cache.match with responses containing "Vary" header');

prepopulated_cache_test(vary_entries, function(cache) {
    return cache.matchAll('http://example.com/c',
                          {ignoreVary: true})
      .then(function(result) {
          assert_array_equivalent(
            result,
            [
              vary_entries.no_vary_header.response,
              vary_entries.vary_cookie_is_cookie.response,
              vary_entries.vary_cookie_is_good.response,
              vary_entries.vary_cookie_absent.response,
              vary_entries.vary_wildcard.response
            ],
            'Cache.matchAll should honor "ignoreVary" parameter.');
        });
  }, 'Cache.matchAll with "ignoreVary" parameter');

cache_test(function(cache) {
    var request = new Request('http://example.com');
    var response;
    var request_url = new URL('simple.txt', location.href).href;
    return fetch(request_url)
      .then(function(fetch_result) {
          response = fetch_result;
          assert_equals(
            response.url, request_url,
            '[https://fetch.spec.whatwg.org/#dom-response-url] ' +
            'Reponse.url should return the URL of the response.');
          return cache.put(request, response);
        })
      .then(function() {
          return cache.match(request.url);
        })
      .then(function(result) {
          assert_object_equals(
            result, response,
            'Cache.match should reutrn a Response object that has the same ' +
            'properties as the stored response.');
        })
      .then(function() {
          return assert_promise_rejects(
            cache.match(response.url),
            'NotFoundError',
            'Cache.match should not match cache entry based on response URL.');
        });
  }, 'Cache.match with Request and Response objects with different URLs');

cache_test(function(cache) {
    var request_url = new URL('simple.txt', location.href).href;
    return fetch(request_url)
      .then(function(fetch_result) {
          return cache.put(new Request(request_url), fetch_result);
        })
      .then(function() {
          return cache.match(request_url);
        })
      .then(function(result) {
          return result.text();
        })
      .then(function(body_text) {
          assert_equals(body_text, 'a simple text file\n',
                        'Cache.match should return a Response object with a ' +
                        'valid body.');
        })
      .then(function() {
          return cache.match(request_url);
        })
      .then(function(result) {
          return result.text();
        })
      .then(function(body_text) {
          assert_equals(body_text, 'a simple text file\n',
                        'Cache.match should return a Response object with a ' +
                        'valid body each time it is called.');
        });
  }, 'Cache.match invoked multiple times for the same Request/Response');

// Helpers ---

// Run |test_function| with a Cache object as its only parameter. Prior to the
// call, the Cache is populated by cache entries from |entries|. The latter is
// expected to be an Object mapping arbitrary keys to objects of the form
// {request: <Request object>, response: <Response object>}.
//
// |test_function| should return a Promise that can be used with promise_test.
function prepopulated_cache_test(entries, test_function, description) {
  cache_test(function(cache) {
      return Promise.all(Object.keys(entries).map(function(k) {
          return cache.put(entries[k].request, entries[k].response);
        }))
        .catch(function(reason) {
            assert_unreached('Test setup failed: ' + reason.message);
          })
        .then(function() {
            return test_function(cache);
          });
    }, description);
}
