// Adapter for testharness.js-style tests with Service Workers

function service_worker_unregister_and_register(test, url, scope) {
  if (!scope || scope.length == 0)
    return Promise.reject(new Error('tests must define a scope'));

  var options = { scope: scope };
  return service_worker_unregister(test, scope)
      .then(function() {
          return navigator.serviceWorker.register(url, options);
        })
      .catch(unreached_rejection(test,
                                 'unregister and register should not fail'));
}

function service_worker_unregister(test, documentUrl) {
  return navigator.serviceWorker.getRegistration(documentUrl)
      .then(function(registration) {
          if (registration)
            return registration.unregister();
        })
      .catch(unreached_rejection(test, 'unregister should not fail'));
}

function service_worker_unregister_and_done(test, scope) {
  return service_worker_unregister(test, scope)
      .then(test.done.bind(test));
}

// Rejection-specific helper that provides more details
function unreached_rejection(test, prefix) {
    return test.step_func(function(error) {
        var reason = error.message || error.name || error;
        var error_prefix = prefix || 'unexpected rejection';
        assert_unreached(error_prefix + ': ' + reason);
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

function unload_iframe(iframe) {
  var saw_unload = new Promise(function(resolve) {
      iframe.contentWindow.addEventListener('unload', function() {
          resolve();
        });
    });
  iframe.src = '';
  iframe.remove();
  return saw_unload;
}

function normalizeURL(url) {
  return new URL(url, document.location).toString().replace(/#.*$/, '');
}

function get_newest_worker(registration) {
  if (!registration) {
    return Promise.reject(new Error(
        'get_newest_worker must be passed a ServiceWorkerRegistration'));
  }
  if (registration.installing)
    return Promise.resolve(registration.installing);
  if (registration.waiting)
    return Promise.resolve(registration.waiting);
  if (registration.active)
    return Promise.resolve(registration.active);
  return Promise.reject(new Error(
      'registration must have at least one version'));
}

function wait_for_update(test, registration) {
  if (!registration || registration.unregister == undefined) {
    return Promise.reject(new Error(
        'wait_for_update must be passed a ServiceWorkerRegistration'));
  }

  return new Promise(test.step_func(function(resolve) {
      registration.addEventListener('updatefound', test.step_func(function() {
          resolve(registration.installing);
      }));
    }));
}

function wait_for_state(test, worker, state) {
  if (!worker || worker.state == undefined) {
    return Promise.reject(new Error(
        'wait_for_state must be passed a ServiceWorker'));
  }
  if (worker.state === state)
    return Promise.resolve(state);

  if (state === 'installing') {
    switch (worker.state) {
    case 'installed':
    case 'activating':
    case 'activated':
    case 'redundant':
      return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  if (state === 'installed') {
    switch (worker.state) {
    case 'activating':
    case 'activated':
    case 'redundant':
      return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  if (state === 'activating') {
    switch (worker.state) {
    case 'activated':
    case 'redundant':
      return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  if (state === 'activated') {
    switch (worker.state) {
    case 'redundant':
      return Promise.reject(new Error(
          'worker is ' + worker.state + ' but waiting for ' + state));
    }
  }

  return new Promise(test.step_func(function(resolve) {
      worker.addEventListener('statechange', test.step_func(function() {
          if (worker.state === state)
            resolve(state);
        }));
    }));
}

function wait_for_activated(test, registration) {
  var expected_state = 'activated';
  if (registration.active)
    return wait_for_state(test, registration.active, expected_state);
  if (registration.waiting)
    return wait_for_state(test, registration.waiting, expected_state);
  if (registration.installing)
    return wait_for_state(test, registration.installing, expected_state);
  return Promise.reject(
      new Error('registration must have at least one version'));
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
    var registration;
    service_worker_unregister_and_register(test, url, scope)
      .then(function(r) {
          registration = r;
          return wait_for_update(test, registration);
        })
      .then(function(worker) { return fetch_tests_from_worker(worker); })
      .then(function() { return registration.unregister(); })
      .then(function() { test.done(); })
      .catch(test.step_func(function(e) { throw e; }));
  };

  self.service_worker_test = service_worker_test;
})();

function get_host_info() {
    var ORIGINAL_HOST = '127.0.0.1';
    var REMOTE_HOST = 'localhost';
    var UNAUTHENTICATED_HOST = 'example.test';
    var HTTP_PORT = 8000;
    var HTTPS_PORT = 8443;
    try {
        // In W3C test, we can get the hostname and port number in config.json
        // using wptserve's built-in pipe.
        // http://wptserve.readthedocs.org/en/latest/pipes.html#built-in-pipes
        HTTP_PORT = eval('{{ports[http][0]}}');
        HTTPS_PORT = eval('{{ports[https][0]}}');
        ORIGINAL_HOST = eval('\'{{host}}\'');
        REMOTE_HOST = 'www1.' + ORIGINAL_HOST;
    } catch(e) {
    }
    return {
        HTTP_ORIGIN: 'http://' + ORIGINAL_HOST + ':' + HTTP_PORT,
        HTTPS_ORIGIN: 'https://' + ORIGINAL_HOST + ':' + HTTPS_PORT,
        HTTP_REMOTE_ORIGIN: 'http://' + REMOTE_HOST + ':' + HTTP_PORT,
        HTTPS_REMOTE_ORIGIN: 'https://' + REMOTE_HOST + ':' + HTTPS_PORT,
        UNAUTHENTICATED_ORIGIN: 'http://' + UNAUTHENTICATED_HOST + ':' + HTTP_PORT
    };
}

function base_path() {
    return location.pathname.replace(/\/[^\/]*$/, '/');
}
