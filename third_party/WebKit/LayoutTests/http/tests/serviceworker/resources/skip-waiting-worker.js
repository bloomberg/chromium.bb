importScripts('worker-testharness.js');

promise_test(function() {
    return self.skipWaiting()
      .then(function(result) {
          assert_equals(result, undefined,
                        'Promise should be resolved with undefined');
        })
      .then(function() {
          var promises = [];
          for (var i = 0; i < 8; ++i)
            promises.push(skipWaiting());
          return Promise.all(promises);
        })
      .then(function(results) {
          results.forEach(function(r) {
              assert_equals(r, undefined,
                            'Promises should be resolved with undefined');
            });
        });
  }, 'skipWaiting');
