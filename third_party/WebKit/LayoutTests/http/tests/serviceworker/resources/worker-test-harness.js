/*
 * worker-test-harness should be considered a temporary polyfill around
 * testharness.js for supporting Service Worker based tests. It should not be
 * necessary once the test harness is able to drive worker based tests natively.
 * See https://github.com/w3c/testharness.js/pull/82 for status of effort to
 * update upstream testharness.js. Once the upstreaming is complete, tests that
 * reference worker-test-harness should be updated to directly import
 * testharness.js.
 */

// The following are necessary to appease attempts by testharness to access the
// DOM.
self.document = {getElementsByTagName: function() { return []; }};
self.window = self;
self.parent = self;

// An onload event handler is used to indicate to the testharness that the
// document has finished loading. At this point the test suite would be
// considered complete if there are no more pending tests and the test isn't
// marked as requring an explicit done() call.
//
// Since ServiceWorkers don't have an onload event, we monkey-patch
// addEventListener to rewire the event to be fired at oninstall which is
// functionally equivalent to onload.
(function() {
  var previous_addEventListener = self.addEventListener;
  self.addEventListener = function() {
    if (arguments.length > 0 && arguments[0] == 'load') {
      arguments[0] = 'install';
    }
    previous_addEventListener.apply(this, arguments);
  };
})();

importScripts('/resources/testharness.js');

(function() {
  // This prevents the worker from attempting to display test results using the
  // DOM.
  setup({output: false});

  // Once the test are considered complete, this logic packages up all the
  // results into a promise resolution so that it can be passed back to the
  // client document when it connects.
  var completion_promise = new Promise(function(resolve, reject) {
      add_completion_callback(function(tests, harness_status) {
          var results = {
            tests: tests.map(function(test) {
                return test.structured_clone();
              }),
            status: harness_status.structured_clone()
          };
          resolve(results);
        });
    });

  // The 'fetch_results' message is sent by the client document to signal that
  // it is now ready to receive test results. It also includes a MessagePort
  // which the worker should use to communicate.
  self.addEventListener('message', function(ev) {
      var message = ev.data;
      if (message.type == 'fetch_results') {
        var port = ev.ports[0];
        completion_promise.then(function(results) {
            var message = {
              type: 'complete',
              tests: results.tests,
              status: results.status
            };
            port.postMessage(message);
          });
      }
    });
})();

// 'promise_test' is a new kind of testharness test that handles some
// boilerplate for testing with promises.
function promise_test(func, name, properties) {
  properties = properties || {};
  var test = async_test(name, properties);
  Promise.resolve(test.step(func, test, test))
    .then(function() { test.done(); })
    .catch(test.step_func(function(value) {
        throw value;
      }));
}

// Returns a promise that fulfills after the provided |promise| is fulfilled.
// The |test| succeeds only if |promise| rejects with an exception matching
// |code|. Accepted values for |code| follow those accepted for assert_throws().
// The optional |description| describes the test being performed.
// E.g.:
//   assert_promise_rejects(
//       new Promise(...), // something that should throw an exception.
//       'NotFoundError',
//       'Should throw NotFoundError.');
//
//   assert_promise_rejects(
//       new Promise(...),
//       new TypeError(),
//       'Should throw TypeError');
function assert_promise_rejects(promise, code, description) {
  return promise.then(
    function() {
      throw 'assert_promise_rejects: ' + description + ' Promise did not throw.';
    },
    function(e) {
      if (code !== undefined) {
        assert_throws(code, function() { throw e; }, description);
      }
    });
}
