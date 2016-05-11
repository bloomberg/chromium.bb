importScripts('/resources/testharness.js');

// The trial should be enabled.
test(() => {
    assert_idl_attribute(self.internals, 'frobulate');
    assert_true(self.internals.frobulate, 'Attribute should return boolean value');
  }, 'Attribute should exist and return value in worker');
done();
