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
        assert_not_exists(window.internals, 'frobulateBindings');
        assert_equals(window.internals['frobulateBindings'], undefined);
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

fetch_tests_from_worker(new Worker('resources/enabled-worker.js'));
};
