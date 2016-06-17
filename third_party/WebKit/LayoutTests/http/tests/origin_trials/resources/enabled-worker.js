importScripts('/resources/testharness.js');

// The trial should be enabled.
test(() => {
    assert_idl_attribute(self.internals, 'frobulate');
    assert_true(self.internals.frobulate, 'Attribute should return boolean value');
  }, 'Attribute should exist and return value in worker');
test(() => {
    assert_idl_attribute(self.internals, 'FROBULATE_CONST');
    assert_equals(self.internals.FROBULATE_CONST, 1, 'Constant should return integer value');
  }, 'Constant should exist and return value in worker');
test(() => {
    assert_idl_attribute(self.internals, 'FROBULATE_CONST');
    self.internals.FROBULATE_CONST = 10;
    assert_equals(self.internals.FROBULATE_CONST, 1, 'Constant should not be modifiable');
  }, 'Constant should exist and not be modifiable in worker');
done();
