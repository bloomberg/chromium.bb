var genericAccessServiceUuid = '00001800-0000-1000-8000-00805f9b34fb';
var genericAttributeServiceUuid = '00001801-0000-1000-8000-00805f9b34fb';
var glucoseServiceUuid = '00001808-0000-1000-8000-00805f9b34fb';
var heartRateServiceUuid = '0000180d-0000-1000-8000-00805f9b34fb';
var batteryServiceUuid = '0000180f-0000-1000-8000-00805f9b34fb';

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
