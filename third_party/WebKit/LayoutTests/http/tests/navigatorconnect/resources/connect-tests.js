// Runs various navigator.connect tests, verifying that connections are accepted
// and rejected in the right circumstances.
// |connect_method| is a function taking a test object as well as the regular
// navigator.connect parameters. This makes it possible to run the same tests
// while wrapping all navigator.connect calls in a helper making the actual
// connection from a cross origin iframe, or a web worker.
function run_connect_tests(connect_method) {
  promise_test(function(t) {
      return assert_promise_rejects(
        connect_method(t, 'https://example.test/service/does/not/exists'),
        'AbortError',
        'navigator.connect should fail with an AbortError');
    }, 'Connection fails to a service that doesn\'t exist.');

  promise_test(function(t) {
      var scope = sw_scope + '/empty';
      var sw_url = 'resources/empty-worker.js';
      return assert_promise_rejects(
        service_worker_unregister_and_register(t, sw_url, scope)
          .then(function(registration) {
              return wait_for_state(t, registration.installing, 'activated');
            })
          .then(function() {
              return connect_method(t, scope + '/service');
            }),
        'AbortError',
        'navigator.connect should fail with an AbortError');
    }, 'Connection fails if service worker doesn\'t handle connect event.');

  promise_test(function(t) {
      var scope = sw_scope + '/rejecting';
      var sw_url = 'resources/rejecting-worker.js';
      return assert_promise_rejects(
        service_worker_unregister_and_register(t, sw_url, scope)
          .then(function(registration) {
              return wait_for_state(t, registration.installing, 'activated');
            })
          .then(function() {
              return connect_method(t, scope + '/service');
            }),
        'AbortError',
        'navigator.connect should fail with an AbortError');
    }, 'Connection fails if service worker rejects connection event.');

  promise_test(function(t) {
      var scope = sw_scope + '/accepting';
      var sw_url = 'resources/accepting-worker.js';
      return service_worker_unregister_and_register(t, sw_url, scope)
        .then(function(registration) {
            return wait_for_state(t, registration.installing, 'activated');
          })
        .then(function() {
            return connect_method(t, scope + '/service');
          })
        .then(function(port) {
            var targetURL = new URL(port.targetURL);
            assert_equals(targetURL.pathname, base_path() + scope + '/service');
            return service_worker_unregister(t, scope);
          });
    }, 'Connection succeeds if service worker accepts connection event.');

  promise_test(function(t) {
      var scope = sw_scope + '/async-rejecting';
      var sw_url = 'resources/async-connect-worker.js';
      return assert_promise_rejects(
        service_worker_unregister_and_register(t, sw_url, scope)
          .then(function(registration) {
              return wait_for_state(t, registration.installing, 'activated');
            })
          .then(function() {
              return connect_method(t, scope + '/service?reject');
            }),
        'AbortError',
        'navigator.connect should fail with an AbortError');
    }, 'Connection fails if service worker rejects connection event async.');

  promise_test(function(t) {
      var scope = sw_scope + '/async-accepting';
      var sw_url = 'resources/async-connect-worker.js';
      return service_worker_unregister_and_register(t, sw_url, scope)
        .then(function(registration) {
            return wait_for_state(t, registration.installing, 'activated');
          })
        .then(function() {
            return connect_method(t, scope + '/service?accept');
          })
        .then(function(port) {
            var targetURL = new URL(port.targetURL);
            assert_equals(targetURL.pathname, base_path() + scope + '/service');
            return service_worker_unregister(t, scope);
          });
    }, 'Connection succeeds if service worker accepts connection event async.');

  promise_test(function(t) {
      var scope = sw_scope + '/accepting-for-name-and-data';
      var sw_url = 'resources/accepting-worker.js';
      return service_worker_unregister_and_register(t, sw_url, scope)
        .then(function(registration) {
            return wait_for_state(t, registration.installing, 'activated');
          })
        .then(function() {
            return connect_method(t, scope + '/service', {name: 'somename', data: 'somedata'});
          })
        .then(function(port) {
            var targetURL = new URL(port.targetURL);
            assert_equals(targetURL.pathname, base_path() + scope + '/service');
            assert_equals(port.name, 'somename');
            assert_equals(port.data, 'somedata');
            return service_worker_unregister(t, scope);
          });
    }, 'Returned port has correct name and data fields');
}
