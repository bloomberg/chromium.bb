importScripts('worker-testharness.js');
importScripts('../../resources/testharness-helpers.js');

promise_test(function(t) {
    var cache_name = 'cache-storage/foo';
    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(cache) {
          assert_true(cache instanceof Cache,
                      'CacheStorage.open should return a Cache.');
        });
  }, 'CacheStorage.open');

promise_test(function(t) {
    // Note that this test may collide with other tests running in the same
    // origin that also uses an empty cache name.
    var cache_name = '';
    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(cache) {
          assert_true(cache instanceof Cache,
                      'CacheStorage.open should accept an empty name.');
        });
  }, 'CacheStorage.open with an empty name');

promise_test(function(t) {
    return assert_promise_rejects(
      self.caches.open(),
      new TypeError(),
      'CacheStorage.open should throw TypeError if called with no arguments.');
  }, 'CacheStorage.open with no arguments');

promise_test(function(t) {
    var test_cases = [
      {
        name: 'cache-storage/lowercase',
        should_not_match:
          [
            'cache-storage/Lowercase',
            ' cache-storage/lowercase',
            'cache-storage/lowercase '
          ]
      },
      {
        name: 'cache-storage/has a space',
        should_not_match:
          [
            'cache-storage/has'
          ]
      },
      {
        name: 'cache-storage/has\000_in_the_name',
        should_not_match:
          [
            'cache-storage/has',
            'cache-storage/has_in_the_name'
          ]
      }
    ];
    return Promise.all(test_cases.map(function(testcase) {
        var cache_name = testcase.name;
        return self.caches.delete(cache_name)
          .then(function() {
              return self.caches.open(cache_name);
            })
          .then(function() {
              return self.caches.has(cache_name);
            })
          .then(function(result) {
              assert_true(result,
                          'CacheStorage.has should return true for existing ' +
                          'cache.');
            })
          .then(function() {
              return Promise.all(
                testcase.should_not_match.map(function(cache_name) {
                    return self.caches.has(cache_name)
                      .then(function(result) {
                          assert_false(result,
                                       'CacheStorage.has should only perform ' +
                                       'exact matches on cache names.');
                        });
                  }));
            })
          .then(function() {
              return self.caches.delete(cache_name);
            });
      }));
  }, 'CacheStorage.has with existing cache');

promise_test(function(t) {
    return self.caches.has('cheezburger')
      .then(function(result) {
          assert_false(result,
                       'CacheStorage.has should return false for ' +
                       'nonexistent cache.');
        });
  }, 'CacheStorage.has with nonexistent cache');

promise_test(function(t) {
    var cache_name = 'cache-storage/open';
    var cache;
    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(result) {
          cache = result;
        })
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(result) {
          assert_equals(result, cache,
                        'CacheStorage.open should return the named Cache ' +
                        'object if it exists.');
        })
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(result) {
          assert_equals(result, cache,
                        'CacheStorage.open should return the same ' +
                        'instance of an existing Cache object.');
        });
  }, 'CacheStorage.open with existing cache');

promise_test(function(t) {
    var cache_name = 'cache-storage/delete';

    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function() { return self.caches.delete(cache_name); })
      .then(function(result) {
          assert_true(result,
                      'CacheStorage.delete should return true after ' +
                      'deleting an existing cache.');
        })

      .then(function() { return self.caches.has(cache_name); })
      .then(function(cache_exists) {
          assert_false(cache_exists,
                       'CacheStorage.has should return false after ' +
                       'fulfillment of CacheStorage.delete promise.');
        });
  }, 'CacheStorage.delete with existing cache');

promise_test(function(t) {
    return self.caches.delete('cheezburger')
      .then(function(result) {
          assert_false(result,
                       'CacheStorage.delete should return false for a ' +
                       'nonexistent cache.');
        });
  }, 'CacheStorage.delete with nonexistent cache');

