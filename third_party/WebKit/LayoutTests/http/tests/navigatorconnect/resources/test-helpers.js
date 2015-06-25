// Helper method that waits for a {success: <boolean>, result: any} reply on
// a port and returns a promise that resolves (if success is true) or rejects
// the promise with the value of the result attribute.
function reply_as_promise(t, port) {
  return new Promise(function(resolve, reject) {
      var got_reply = false;
      port.onmessage = t.step_func(function(event) {
        assert_false(got_reply);
        assert_true('success' in event.data);
        assert_true('result' in event.data);
        got_reply = true;
        if (event.data.success)
          resolve(event.data.result);
        else
          reject(event.data.result);
      });
    });
}

// Method that behaves similarly to navigator.connect, but the actual connect
// call is made from a cross origin iframe.
function cross_origin_connect(t, service) {
  // |service| could be a relative URL, but for this to work from the iframe it
  // needs an absolute URL.
  var target_url = new URL(service, location.origin + base_path());
  return with_iframe(
      cross_origin + base_path() + 'resources/connect-helper.html')
    .then(function(iframe) {
        var channel = new MessageChannel();
        iframe.contentWindow.postMessage(
          {connect: target_url.href, port: channel.port2}, '*', [channel.port2]);
        return reply_as_promise(t, channel.port1);
    });
}

// Method that behaves similarly to navigator.connect, but the actual connect
// call is made from a worker.
function connect_from_worker(t, service) {
  // |service| is a relative URL, but for this to work from the worker it needs
  // an absolute URL.
  var target_url = location.origin + base_path() + service;
  var worker = new Worker('resources/connect-helper.js');
  var channel = new MessageChannel();
  worker.postMessage
    ({connect: target_url, port: channel.port2}, [channel.port2]);
  return reply_as_promise(t, channel.port1);
}

// Similar to Promise.race, except that returned promise only rejects if all
// passed promises reject. Used temporarily to support both old and new client
// side APIs.
function first_to_resolve(promises) {
  return new Promise(function(resolve, reject) {
    var remaining = promises.length;
    var resolved = false;
    for (var i = 0; i < promises.length; ++i) {
      Promise.resolve(promises[i])
        .then(function(result) {
            if (!resolved) {
              resolve(result);
              resolved = true;
            }
          })
        .catch(function(result) {
            remaining--;
            if (remaining === 0) {
              reject(result);
            }
          });
    }
  });
}

// Takes (a promise resolving to) a ServicePort instance, and returns a Promise
// that resolves to a MessagePort wrapping that ServicePort. Used to support
// both old and new APIs at the same time.
function wrap_in_port(maybe_port) {
  return Promise.resolve(maybe_port).then(
    function(port) {
      var channel = new MessageChannel();
      channel.port2.onmessage = function(event) {
        port.postMessage(event.data, event.ports);
      };
      // Should use addEventListener and check source of event, but source isn't
      // set yet, so for now just assume only one wrapped port is used at a time.
      navigator.services.onmessage = function(event) {
        channel.port2.postMessage(event.data, event.ports);
      };
      return channel.port1;
    }
  );
}

var promise_tests = Promise.resolve();
// Helper function to run promise tests one after the other.
// TODO(ortuno): Remove once https://github.com/w3c/testharness.js/pull/115/files
// gets through.
function sequential_promise_test(func, name) {
  var test = async_test(name);
  promise_tests = promise_tests.then(function() {
    return test.step(func, test, test);
  }).then(function() {
    test.done();
  }).catch(test.step_func(function(value) {
    // step_func catches the error again so the error doesn't propagate.
    throw value;
  }));
}
