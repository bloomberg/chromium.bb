// The sample API integrates origin trial checks at various entry points.

// These tests verify that any gated parts of the API are not available.
expect_failure = (t) => {
  tests = [{
    desc: 'Accessing attribute should throw error',
    code: () => {
        assert_idl_attribute(window.internals, 'frobulate');
        assert_throws("NotSupportedError", () => { window.internals.frobulate; },
            'Accessing attribute should throw error');
      }
  }, {
    desc: 'Attribute should exist and return value, with trial disabled',
    code: () => {
        assert_idl_attribute(window.internals, 'frobulateNoEnabledCheck');
        assert_true(window.internals.frobulateNoEnabledCheck,
            'Attribute should return boolean value');
      }
  }, {
    desc: 'Attribute should not exist, with trial disabled',
    code: () => {
        assert_false('frobulateBindings' in window.internals);
        assert_not_exists(window.internals, 'frobulateBindings');
        assert_equals(window.internals['frobulateBindings'], undefined);
      }
  }, {
    desc: 'Constant should not exist, with trial disabled',
    code: () => {
        assert_false('FROBULATE_CONST' in window.internals);
        assert_not_exists(window.internals, 'FROBULATE_CONST');
        assert_equals(window.internals['FROBULATE_CONST'], undefined);
      }
  }, {
    desc: 'Attribute should not exist on partial interface, with trial disabled',
    code: () => {
        assert_not_exists(window.internals, 'frobulatePartial');
        assert_equals(window.internals['frobulatePartial'], undefined);
      }
  }, {
    desc: 'Constant should not exist on partial interface, with trial disabled',
    code: () => {
        assert_false('FROBULATE_CONST_PARTIAL' in window.internals);
        assert_not_exists(window.internals, 'FROBULATE_CONST_PARTIAL');
        assert_equals(window.internals['FROBULATE_CONST_PARTIAL'], undefined);
      }
  }, {
    desc: 'Method should not exist on partial interface, with trial disabled',
    code: () => {
        assert_false('frobulateMethodPartial' in window.internals);
        assert_not_exists(window.internals, 'frobulateMethodPartial');
        assert_equals(window.internals['frobulateMethodPartial'], undefined);
      }
  }, {
    desc: 'Static method should not exist on partial interface, with trial disabled',
    code: () => {
        var internalsInterface = window.internals.constructor;
        assert_false('frobulateStaticMethodPartial' in internalsInterface);
        assert_not_exists(internalsInterface, 'frobulateStaticMethodPartial');
        assert_equals(internalsInterface['frobulateStaticMethodPartial'], undefined);
      }
  }];

  fetch_tests_from_worker(new Worker('resources/disabled-worker.js'));

  for (var i = 0; i < tests.length; ++i) {
    if (t)
      t.step(tests[i].code);
    else
      test(tests[i].code, tests[i].desc);
  }
};


// These tests verify that the API functions correctly with an enabled trial.
expect_success = () => {
test(() => {
    assert_idl_attribute(window.internals, 'frobulate');
    assert_true(window.internals.frobulate, 'Attribute should return boolean value');
  }, 'Attribute should exist and return value');

test(() => {
    assert_idl_attribute(window.internals, 'frobulateBindings');
    assert_true(window.internals.frobulateBindings, 'Attribute should return boolean value');
  }, 'Attribute should exist and return value');

test(() => {
    assert_idl_attribute(window.internals, 'FROBULATE_CONST');
    assert_equals(window.internals.FROBULATE_CONST, 1, 'Constant should return integer value');
  }, 'Constant should exist and return value');

test(() => {
    assert_idl_attribute(window.internals, 'FROBULATE_CONST');
    window.internals.FROBULATE_CONST = 10;
    assert_equals(window.internals.FROBULATE_CONST, 1, 'Constant should not be modifiable');
  }, 'Constant should exist and not be modifiable');

test(() => {
    assert_idl_attribute(window.internals, 'frobulatePartial');
    assert_true(window.internals.frobulatePartial, 'Attribute should return boolean value');
  }, 'Attribute should exist on partial interface and return value');

test(() => {
    assert_idl_attribute(window.internals, 'FROBULATE_CONST_PARTIAL');
    assert_equals(window.internals.FROBULATE_CONST_PARTIAL, 2, 'Constant should return integer value');
  }, 'Constant should exist on partial interface and return value');

test(() => {
    assert_idl_attribute(window.internals, 'frobulateMethodPartial');
    assert_true(window.internals.frobulateMethodPartial(), 'Method should return boolean value');
  }, 'Method should exist on partial interface and return value');

test(() => {
    var internalsInterface = window.internals.constructor;
    assert_exists(internalsInterface, 'frobulateStatic');
    assert_true(internalsInterface.frobulateStatic, 'Static attribute should return boolean value');
  }, 'Static attribute should exist on partial interface and return value');

test(() => {
    var internalsInterface = window.internals.constructor;
    assert_exists(internalsInterface, 'frobulateStaticMethodPartial');
    assert_true(internalsInterface.frobulateStaticMethodPartial(), 'Static method should return boolean value');
  }, 'Static method should exist on partial interface and return value');

fetch_tests_from_worker(new Worker('resources/enabled-worker.js'));
};
