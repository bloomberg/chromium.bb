// The sample API integrates origin trial checks at various entry points.

// These tests verify that any gated parts of the API are not available.
expect_failure = () => {
test(() => {
    assert_idl_attribute(window.internals, 'frobulate');
    assert_throws("NotSupportedError", () => { window.internals.frobulate; },
       'Accessing attribute should throw error');
  }, 'Attribute should throw API unavailable error');

test(() => {
    assert_idl_attribute(window.internals, 'frobulateNoEnabledCheck');
    assert_true(window.internals.frobulateNoEnabledCheck,
        'Attribute should return boolean value');
  }, 'Attribute should exist and return value, with trial disabled');
};


// These tests verify that the API functions correctly with an enabled trial.
expect_success = () => {
test(() => {
    assert_idl_attribute(window.internals, 'frobulate');
    assert_true(window.internals.frobulate, 'Attribute should return boolean value');
  }, 'Attribute should exist and return value');
};
