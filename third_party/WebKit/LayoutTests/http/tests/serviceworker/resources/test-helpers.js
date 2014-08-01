// Adapter for testharness.js-style tests with Service Workers

function service_worker_unregister_and_register(test, url, scope) {
    var options = scope ? { scope: scope } : {};
    return navigator.serviceWorker.unregister(scope).then(
        test.step_func(function() {
            return navigator.serviceWorker.register(url, options);
        }),
        unreached_rejection(test, 'Unregister should not fail')
    ).then(test.step_func(function(worker) {
          return Promise.resolve(worker);
        }),
        unreached_rejection(test, 'Registration should not fail')
    );
}

function service_worker_unregister_and_done(test, scope) {
    return navigator.serviceWorker.unregister(scope).then(
        test.done.bind(test),
        unreached_rejection(test, 'Unregister should not fail'));
}

// Rejection-specific helper that provides more details
function unreached_rejection(test, prefix) {
    return test.step_func(function(error) {
        var reason = error.name ? error.name : error;
        var prefix = prefix ? prefix : "unexpected rejection";
        assert_unreached(prefix + ': ' + reason);
    });
}

// FIXME: Clean up the iframe when the test completes.
function with_iframe(url, f) {
    return new Promise(function(resolve, reject) {
        var frame = document.createElement('iframe');
        frame.src = url;
        frame.onload = function() {
            if (f) {
              f(frame);
            }
            resolve(frame);
        };
        document.body.appendChild(frame);
    });
}

function normalizeURL(url) {
  return new URL(url, document.location).toString().replace(/#.*$/, '');
}

function wait_for_state(test, worker, state) {
    return new Promise(test.step_func(function(resolve, reject) {
        worker.addEventListener('statechange', test.step_func(function() {
            if (worker.state === state)
                resolve(state);
        }));
    }));
}

(function() {
  function fetch_tests_from_worker(worker) {
    return new Promise(function(resolve, reject) {
        var messageChannel = new MessageChannel();
        messageChannel.port1.addEventListener('message', function(message) {
            if (message.data.type == 'complete') {
              synthesize_tests(message.data.tests, message.data.status);
              resolve();
            }
          });
        worker.postMessage({type: 'fetch_results', port: messageChannel.port2},
                           [messageChannel.port2]);
        messageChannel.port1.start();
      });
  };

  function synthesize_tests(tests, status) {
    for (var i = 0; i < tests.length; ++i) {
      var t = async_test(tests[i].name);
      switch (tests[i].status) {
        case tests[i].PASS:
          t.step(function() { assert_true(true); });
          t.done();
          break;
        case tests[i].TIMEOUT:
          t.force_timeout();
          break;
        case tests[i].FAIL:
          t.step(function() { throw {message: tests[i].message}; });
          break;
        case tests[i].NOTRUN:
          // Leave NOTRUN alone. It'll get marked as a NOTRUN when the test
          // terminates.
          break;
      }
    }
  };

  function service_worker_test(url, description) {
    var scope = window.location.origin + '/service-worker-scope/' +
      window.location.pathname;

    var test = async_test(description);
    service_worker_unregister_and_register(test, url, scope)
      .then(function(worker) { return fetch_tests_from_worker(worker); })
      .then(function() { return navigator.serviceWorker.unregister(scope); })
      .then(function() { test.done(); })
      .catch(test.step_func(function(e) { throw e; }));
  };

  self.service_worker_test = service_worker_test;
})();
