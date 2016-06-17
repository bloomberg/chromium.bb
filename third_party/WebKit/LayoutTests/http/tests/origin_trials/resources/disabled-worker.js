importScripts('/resources/testharness.js');

// The trial should not be enabled.
test(() => {
    assert_not_exists(self.internals, 'frobulate');
    assert_equals(self.internals.frobulate, undefined);
  }, 'Attribute should not exist in worker');
test(() => {
    assert_not_exists(self.internals, 'FROBULATE_CONST');
    assert_equals(self.internals.FROBULATE_CONST, undefined);
  }, 'Constant should not exist in worker');
done();
