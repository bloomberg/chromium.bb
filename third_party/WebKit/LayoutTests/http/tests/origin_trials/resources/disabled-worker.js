importScripts('/resources/testharness.js');

// Test whether the origin-trial-enabled attributes are *NOT* attached in a
// worker where the trial is not enabled.
// This is deliberately just a minimal set of tests to ensure that trials are
// available in a worker. The full suite of tests are in origin_trials.js.
test(() => {
  var testObject = self.internals.originTrialsTest();
  assert_idl_attribute(testObject, 'throwingAttribute');
  assert_throws('NotSupportedError', () => {
    testObject.throwingAttribute;
  }, 'Accessing attribute should throw error');
}, 'Accessing attribute should throw error in worker');
test(() => {
  var testObject = self.internals.originTrialsTest();
  assert_not_exists(testObject, 'normalAttribute');
  assert_equals(testObject.normalAttribute, undefined);
}, 'Attribute should not exist in worker');
test(() => {
  var testObject = self.internals.originTrialsTest();
  assert_not_exists(testObject, 'CONSTANT');
  assert_equals(testObject.CONSTANT, undefined);
}, 'Constant should not exist in worker');
done();
