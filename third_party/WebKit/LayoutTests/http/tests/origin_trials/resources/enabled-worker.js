importScripts('/resources/testharness.js');

// Test whether the origin-trial-enabled attributes are attached in a worker
// where the trial is enabled.
// This is deliberately just a minimal set of tests to ensure that trials are
// available in a worker. The full suite of tests are in origin_trials.js.
test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'throwingAttribute');
    assert_true(testObject.throwingAttribute, 'Attribute should return boolean value');
  }, 'Attribute should exist and not throw exception in worker');
test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'normalAttribute');
    assert_true(testObject.normalAttribute, 'Attribute should return boolean value');
  }, 'Attribute should exist and return value in worker');
test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'CONSTANT');
    assert_equals(testObject.CONSTANT, 1, 'Constant should return integer value');
  }, 'Constant should exist and return value in worker');
test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'CONSTANT');
    testObject.CONSTANT = 10;
    assert_equals(testObject.CONSTANT, 1, 'Constant should not be modifiable');
  }, 'Constant should exist and not be modifiable in worker');
done();
