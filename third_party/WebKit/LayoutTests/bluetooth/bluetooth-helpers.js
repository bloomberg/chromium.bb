var promise_tests = Promise.resolve();
// Helper function to run promise tests one after the other.
// TODO(ortuno): Remove once https://github.com/w3c/testharness.js/pull/115/files
// gets through.
function sequential_promise_test(func, name) {
  var test = async_test(name);
  promise_tests = promise_tests.then(function() {
    return Promise.resolve(test.step(func, test, test))
      .then(function() {
        test.done();
      })
      .catch(test.step_func(
        function(value) {
          throw value;
        }));
  });
}
