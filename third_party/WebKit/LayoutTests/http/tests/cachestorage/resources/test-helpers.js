(function() {
  var next_cache_index = 1;

  // Returns a promise that resolves to a newly created Cache object. The
  // returned Cache will be destroyed when |test| completes.
  function create_temporary_cache(test) {
    var uniquifier = String(++next_cache_index);
    var cache_name = self.location.pathname + '/' + uniquifier;

    test.add_cleanup(function() {
        self.caches.delete(cache_name);
      });

    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        });
  }

  self.create_temporary_cache = create_temporary_cache;
})();

// Runs |test_function| with a temporary unique Cache passed in as the only
// argument. The function is run as a part of Promise chain owned by
// promise_test(). As such, it is expected to behave in a manner identical (with
// the exception of the argument) to a function passed into promise_test().
//
// E.g.:
//    cache_test(function(cache) {
//      // Do something with |cache|, which is a Cache object.
//    }, "Some Cache test");
function cache_test(test_function, description) {
  promise_test(function(test) {
      return create_temporary_cache(test)
        .then(test_function);
    }, description);
}

// A set of Request/Response pairs to be used with prepopulated_cache_test().
var simple_entries = [
  {
    name: 'a',
    request: new Request('http://example.com/a'),
    response: new Response('')
  },

  {
    name: 'b',
    request: new Request('http://example.com/b'),
    response: new Response('')
  },

  {
    name: 'a_with_query',
    request: new Request('http://example.com/a?q=r'),
    response: new Response('')
  },

  {
    name: 'A',
    request: new Request('http://example.com/A'),
    response: new Response('')
  },

  {
    name: 'a_https',
    request: new Request('https://example.com/a'),
    response: new Response('')
  },

  {
    name: 'a_org',
    request: new Request('http://example.org/a'),
    response: new Response('')
  },

  {
    name: 'cat',
    request: new Request('http://example.com/cat'),
    response: new Response('')
  },

  {
    name: 'catmandu',
    request: new Request('http://example.com/catmandu'),
    response: new Response('')
  },

  {
    name: 'cat_num_lives',
    request: new Request('http://example.com/cat?lives=9'),
    response: new Response('')
  },

  {
    name: 'cat_in_the_hat',
    request: new Request('http://example.com/cat/in/the/hat'),
    response: new Response('')
  },

  {
    name: 'secret_cat',
    request: new Request('http://tom:jerry@example.com/cat'),
    response: new Response('')
  },

  {
    name: 'top_secret_cat',
    request: new Request('http://tom:j3rry@example.com/cat'),
    response: new Response('')
  },
  {
    name: 'non_2xx_response',
    request: new Request('http://example.com/non2xx'),
    response: new Response('', {status: 404, statusText: 'nope'})
  },

  {
    name: 'error_response',
    request: new Request('http://example.com/error'),
    response: Response.error()
  },
];

// A set of Request/Response pairs to be used with prepopulated_cache_test().
// These contain a mix of test cases that use Vary headers.
var vary_entries = [
  {
    name: 'vary_cookie_is_cookie',
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'is-for-cookie'}}),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  {
    name: 'vary_cookie_is_good',
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'is-good-enough-for-me'}}),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  {
    name: 'vary_cookie_absent',
    request: new Request('http://example.com/c'),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  {
    name: 'vary_wildcard',
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'x', 'X-Key': '1'}}),
    response: new Response('',
                           {headers: {'Vary': '*'}})
  }
];

// Run |test_function| with a Cache object and a map of entries. Prior to the
// call, the Cache is populated by cache entries from |entries|. The latter is
// expected to be an Object mapping arbitrary keys to objects of the form
// {request: <Request object>, response: <Response object>}. There's no
// guarantee on the order in which entries will be added to the cache.
//
// |test_function| should return a Promise that can be used with promise_test.
function prepopulated_cache_test(entries, test_function, description) {
  cache_test(function(cache) {
      var p = Promise.resolve();
      var hash = {};
      return Promise.all(entries.map(function(entry) {
          hash[entry.name] = entry;
          return cache.put(entry.request.clone(),
                           entry.response.clone())
            .catch(function(e) {
                assert_unreached(
                  'Test setup failed for entry ' + entry.name + ': ' + e);
            });
        }))
        .then(function() {
            assert_equals(Object.keys(hash).length, entries.length);
        })
        .then(function() {
            return test_function(cache, hash);
        });
    }, description);
}
