// Runs various navigator.connect tests, verifying that message are sent and
// received correctly.
// |connect_method| is a function taking a test object as well as the regular
// navigator.connect parameters. This makes it possible to run the same tests
// while wrapping all navigator.connect calls in a helper making the actual
// connection from a cross origin iframe, or a web worker.
function run_postmessage_tests(source_origin, connect_method) {
  // Helper method that waits for a reply on a port, and resolves a promise with
  // the reply.
  function wait_for_reply(t, port) {
    return new Promise(function(resolve) {
      var resolved = false;
      port.onmessage = t.step_func(function(event) {
        assert_false(resolved);
        resolved = true;
        resolve(event.data);
      });
    });
  }

  // FIXME: Current navigator.connect implementation returns origins with a
  // trailing slash, which is probably not correct.
  if (source_origin.charAt(source_origin.length - 1) != '/')
    source_origin += '/';

  promise_test(function(t) {
      var scope = sw_scope + '/echo';
      var sw_url = 'resources/echo-worker.js';
      var test_message = 'ping over navigator.connect';
      return service_worker_unregister_and_register(t, sw_url, scope)
        .then(function(registration) {
            return wait_for_state(t, registration.installing, 'activated');
          })
        .then(function() {
            return connect_method(t, scope + '/service');
          })
        .then(function(port) {
            port.postMessage(test_message);
            return wait_for_reply(t, port);
          })
        .then(function(response) {
            assert_equals(response, test_message);
            return service_worker_unregister(t, scope);
          });
    }, 'Messages can be sent and received.');

  promise_test(function(t) {
      var scope = sw_scope + '/echo-port';
      var sw_url = 'resources/echo-worker.js';
      var test_message = 'ping over navigator.connect';
      var channel = new MessageChannel();
      return service_worker_unregister_and_register(t, sw_url, scope)
        .then(function(registration) {
            return wait_for_state(t, registration.installing, 'activated');
        })
      .then(function() {
            return connect_method(t, scope + '/service');
        })
      .then(function(port) {
          channel.port1.postMessage(test_message);
          port.postMessage({port: channel.port2}, [channel.port2]);
          return wait_for_reply(t, port);
        })
      .then(function(response) {
          assert_true('port' in response);
          assert_class_string(response.port, 'MessagePort');
          return wait_for_reply(t, response.port);
        })
      .then(function(response) {
          assert_equals(response, test_message);
          return service_worker_unregister(t, scope);
        });
  }, 'Ports can be sent and received.');

  promise_test(function(t) {
      var scope = sw_scope + '/reply-client-info';
      var sw_url = 'resources/reply-client-info-worker.js';
      var target_url = scope + '/service';
      var port;
      return service_worker_unregister_and_register(t, sw_url, scope)
        .then(function(registration) {
            return wait_for_state(t, registration.installing, 'activated');
          })
        .then(function() {
            return connect_method(t, target_url);
          })
        .then(function(p) {
            port = p;
            return wait_for_reply(t, port)
          })
        .then(function(response) {
            assert_equals(response.targetUrl,
                          location.origin + base_path() + target_url);
            assert_equals(response.origin, source_origin);
            var r = wait_for_reply(t, port);
            port.postMessage('ping');
            return r;
          })
        .then(function(response) {
            assert_equals(response.origin, source_origin);
            return service_worker_unregister(t, scope);
          });
    }, 'Connect and message events include expected origin and targetUrl.');
}
