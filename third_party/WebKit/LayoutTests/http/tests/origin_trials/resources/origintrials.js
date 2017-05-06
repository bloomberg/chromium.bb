// The sample API integrates origin trial checks at various entry points.
// References to "partial interface" mean that the [OriginTrialEnabled]
// IDL attribute is applied to an entire partial interface, instead of
// applied to individual IDL members.

// Verify that the given member exists, and returns an actual value
// (i.e. not undefined).
expect_member = (member_name, get_value_func) => {
  var testObject = window.internals.originTrialsTest();
  assert_idl_attribute(testObject, member_name);
  assert_true(get_value_func(testObject),
    'Member should return boolean value');
}

// Verify that the given static member exists, and returns an actual value
// (i.e. not undefined).
expect_static_member = (member_name, get_value_func) => {
  var testObject = window.internals.originTrialsTest();
  var testInterface = testObject.constructor;
  assert_exists(testInterface, member_name);
  assert_true(get_value_func(testInterface),
    'Static member should return boolean value');
}

// Verify that the given constant exists, and returns the expected value, and
// is not modifiable.
expect_constant = (constant_name, constant_value, get_value_func) => {
  var testObject = window.internals.originTrialsTest();
  var testInterface = testObject.constructor;
  assert_exists(testInterface, constant_name);
  assert_equals(get_value_func(testInterface), constant_value,
    'Constant should return expected value');
  testInterface[constant_name] = constant_value + 1;
  assert_equals(get_value_func(testInterface), constant_value,
    'Constant should not be modifiable');
}

// Verify that given static member does not exist, and does not provide a value
// (i.e. is undefined).
expect_member_fails = (member_name) => {
  var testObject = window.internals.originTrialsTest();
  assert_false(member_name in testObject);
  assert_not_exists(testObject, member_name);
  assert_equals(testObject[member_name], undefined);
}

// Verify that given member does not exist, and does not provide a value
// (i.e. is undefined).
expect_static_member_fails = (member_name) => {
  var testObject = window.internals.originTrialsTest();
  var testInterface = testObject.constructor;
  assert_false(member_name in testInterface);
  assert_not_exists(testInterface, member_name);
  assert_equals(testInterface[member_name], undefined);
}

// These tests verify that any gated parts of the API are not available.
expect_failure = (skip_worker) => {
  tests = [{
    desc: 'Accessing attribute should throw error',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        assert_idl_attribute(testObject, 'throwingAttribute');
        assert_throws("NotSupportedError", () => { testObject.throwingAttribute; },
            'Accessing attribute should throw error');
      }
  }, {
    desc: 'Attribute should exist and return value, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        assert_idl_attribute(testObject, 'unconditionalAttribute');
        assert_true(testObject.unconditionalAttribute,
            'Attribute should return boolean value');
      }
  }, {
    desc: 'Attribute should not exist, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        assert_false('normalAttribute' in testObject);
        assert_not_exists(testObject, 'normalAttribute');
        assert_equals(testObject['normalAttribute'], undefined);
      }
  }, {
    desc: 'Constant should not exist, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        var testInterface = testObject.constructor;
        assert_false('CONSTANT' in testInterface);
        assert_not_exists(testInterface, 'CONSTANT');
        assert_equals(testInterface['CONSTANT'], undefined);
      }
  }, {
    desc: 'Attribute should not exist on partial interface, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        var testInterface = testObject.constructor;
        assert_not_exists(testObject, 'normalAttributePartial');
        assert_equals(testObject['normalAttributePartial'], undefined);
      }
  }, {
    desc: 'Static attribute should not exist on partial interface, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        var testInterface = testObject.constructor;
        assert_false('staticAttributePartial' in testInterface);
        assert_not_exists(testInterface, 'staticAttributePartial');
        assert_equals(testInterface['staticAttributePartial'], undefined);
      }
  }, {
    desc: 'Constant should not exist on partial interface, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        var testInterface = testObject.constructor;
        assert_false('CONSTANT_PARTIAL' in testInterface);
        assert_not_exists(testInterface, 'CONSTANT_PARTIAL');
        assert_equals(testInterface['CONSTANT_PARTIAL'], undefined);
      }
  }, {
    desc: 'Method should not exist on partial interface, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        assert_false('methodPartial' in testObject);
        assert_not_exists(testObject, 'methodPartial');
        assert_equals(testObject['methodPartial'], undefined);
      }
  }, {
    desc: 'Static method should not exist on partial interface, with trial disabled',
    code: () => {
        var testObject = window.internals.originTrialsTest();
        var testInterface = testObject.constructor;
        assert_false('staticMethodPartial' in testInterface);
        assert_not_exists(testInterface, 'staticMethodPartial');
        assert_equals(testInterface['staticMethodPartial'], undefined);
      }
  }];

  if (!skip_worker) {
    fetch_tests_from_worker(new Worker('resources/disabled-worker.js'));
  }

  for (var i = 0; i < tests.length; ++i) {
    test(tests[i].code, tests[i].desc);
  }
};

// These tests verify that the API functions correctly with an enabled trial.
expect_success = () => {
test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'throwingAttribute');
    assert_true(testObject.throwingAttribute, 'Attribute should return boolean value');
  }, 'Accessing attribute should return value and not throw exception');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'normalAttribute');
    assert_true(testObject.normalAttribute, 'Attribute should return boolean value');
  }, 'Attribute should exist and return value');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    var testInterface = testObject.constructor;
    assert_exists(testInterface, 'CONSTANT');
    assert_equals(testInterface.CONSTANT, 1, 'Constant should return integer value');
  }, 'Constant should exist on interface and return value');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    var testInterface = testObject.constructor;
    assert_exists(testInterface, 'CONSTANT');
    testInterface.CONSTANT = 10;
    assert_equals(testInterface.CONSTANT, 1, 'Constant should not be modifiable');
  }, 'Constant should exist on interface and not be modifiable');
test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'normalAttributePartial');
    assert_true(testObject.normalAttributePartial, 'Attribute should return boolean value');
  }, 'Attribute should exist on partial interface and return value');
test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    var testInterface = testObject.constructor;
    assert_exists(testInterface, 'staticAttributePartial');
    assert_true(testInterface.staticAttributePartial, 'Static attribute should return boolean value');
  }, 'Static attribute should exist on partial interface and return value');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    var testInterface = testObject.constructor;
    assert_exists(testInterface, 'CONSTANT_PARTIAL');
    assert_equals(testInterface.CONSTANT_PARTIAL, 2, 'Constant should return integer value');
  }, 'Constant should exist on partial interface and return value');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'normalMethodPartial');
    assert_true(testObject.normalMethodPartial(), 'Method should return boolean value');
  }, 'Method should exist on partial interface and return value');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    var testObjectInterface = testObject.constructor;
    assert_exists(testObjectInterface, 'staticMethodPartial');
    assert_true(testObjectInterface.staticMethodPartial(), 'Static method should return boolean value');
  }, 'Static method should exist on partial interface and return value');
test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var test_object = window.internals.originTrialsTest();
    assert_idl_attribute(test_object, 'normalAttribute');
    assert_true(test_object.normalAttribute, 'Attribute should return boolean value');
  }, 'Attribute should exist on interface and return value');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    var testInterface = testObject.constructor;
    assert_exists(testInterface, 'staticAttribute');
    assert_true(testInterface.staticAttribute, 'Static attribute should return boolean value');
  }, 'Static attribute should exist on interface and return value');

fetch_tests_from_worker(new Worker('resources/enabled-worker.js'));
};

// These tests should pass, regardless of the state of the trial. These are
// control tests for IDL members without the [OriginTrialEnabled] extended
// attribute. The control tests will vary for secure vs insecure context.
expect_always_bindings = (insecure_context, opt_description_suffix) => {
  var description_suffix = opt_description_suffix || '';

  test(() => {
      assert_idl_attribute(window.internals, 'originTrialsTest');
    }, 'Test object should exist on window.internals, regardless of trial' + description_suffix);

  test(() => {
      expect_member('unconditionalAttribute', (testObject) => {
          return testObject.unconditionalAttribute;
        });
    }, 'Attribute should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_static_member('staticUnconditionalAttribute', (testObject) => {
          return testObject.staticUnconditionalAttribute;
        });
    }, 'Static attribute should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_member('unconditionalMethod', (testObject) => {
          return testObject.unconditionalMethod();
        });
    }, 'Method should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_static_member('staticUnconditionalMethod', (testObject) => {
          return testObject.staticUnconditionalMethod();
        });
    }, 'Static method should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_constant('UNCONDITIONAL_CONSTANT', 99, (testObject) => {
          return testObject.UNCONDITIONAL_CONSTANT;
        });
    }, 'Constant should exist on interface and return value, regardless of trial' + description_suffix);

  if (insecure_context) {
    // TODO(crbug.com/695123): Uncomment test when fixed so [SecureContext] and
    //   [OriginTrialEnabled] extended attributes work correctly together.
    /*
    test(() => {
        expect_member_fails('secureUnconditionalAttribute');
      }, 'Secure attribute should not exist, regardless of trial' + description_suffix);
    */
    test(() => {
        expect_member_fails('secureStaticUnconditionalAttribute');
      }, 'Secure static attribute should not exist, regardless of trial' + description_suffix);
    // TODO(crbug.com/695123): Uncomment test when fixed so [SecureContext] and
    //   [OriginTrialEnabled] extended attributes work correctly together.
    /*
    test(() => {
        expect_member_fails('secureUnconditionalMethod');
      }, 'Secure method should not exist, regardless of trial' + description_suffix);
    */
    test(() => {
        expect_member_fails('secureStaticUnconditionalMethod');
      }, 'Secure static method should not exist, regardless of trial' + description_suffix);
  } else {
    test(() => {
        expect_member('secureUnconditionalAttribute', (testObject) => {
            return testObject.secureUnconditionalAttribute;
          });
      }, 'Secure attribute should exist and return value, regardless of trial' + description_suffix);

    test(() => {
        expect_static_member('secureStaticUnconditionalAttribute', (testObject) => {
            return testObject.secureStaticUnconditionalAttribute;
          });
      }, 'Secure static attribute should exist and return value, regardless of trial' + description_suffix);

    test(() => {
        expect_member('secureUnconditionalMethod', (testObject) => {
            return testObject.secureUnconditionalMethod();
          });
      }, 'Secure method should exist and return value, regardless of trial' + description_suffix);

    test(() => {
        expect_static_member('secureStaticUnconditionalMethod', (testObject) => {
            return testObject.secureStaticUnconditionalMethod();
          });
      }, 'Secure static method should exist and return value, regardless of trial' + description_suffix);
  }
};

// Verify that all IDL members are correctly exposed with an enabled trial.
expect_success_bindings = (insecure_context) => {
  expect_always_bindings(insecure_context);

  if (insecure_context) {
    // Origin trials only work in secure contexts, so tests cannot distinguish
    // between [OriginTrialEnabled] or [SecureContext] preventing exposure of
    // IDL members. These tests at least ensure IDL members are not exposed in
    // insecure contexts, regardless of reason.
    test(() => {
        expect_member_fails('secureAttribute');
      }, 'Secure attribute should not exist');
    test(() => {
        expect_static_member_fails('secureStaticAttribute');
      }, 'Secure static attribute should not exist');
    test(() => {
        expect_member_fails('secureMethod');
      }, 'Secure method should not exist');
    test(() => {
        expect_static_member_fails('secureStaticMethod');
      }, 'Secure static method should not exist');
    test(() => {
        expect_member_fails('secureAttributePartial');
      }, 'Secure attribute should not exist on partial interface');
    test(() => {
        expect_static_member_fails('secureStaticAttributePartial');
      }, 'Secure static attribute should not exist on partial interface');
    test(() => {
        expect_member_fails('secureMethodPartial');
      }, 'Secure method should not exist on partial interface');
    test(() => {
        expect_static_member_fails('secureStaticMethodPartial');
      }, 'Secure static method should not exist on partial interface');
  } else {
    test(() => {
        expect_member('normalMethod', (testObject) => {
            return testObject.normalMethod();
          });
      }, 'Method should exist and return value');

    test(() => {
        expect_static_member('staticMethod', (testObject) => {
            return testObject.staticMethod();
          });
      }, 'Static method should exist and return value');

    // Tests for combination of [OriginTrialEnabled] and [SecureContext]
    test(() => {
        expect_member('secureAttribute', (testObject) => {
            return testObject.secureAttribute;
          });
      }, 'Secure attribute should exist and return value');

    test(() => {
        expect_static_member('secureStaticAttribute', (testObject) => {
            return testObject.secureStaticAttribute;
          });
      }, 'Secure static attribute should exist and return value');

    test(() => {
        expect_member('secureMethod', (testObject) => {
            return testObject.secureMethod();
          });
      }, 'Secure method should exist and return value');

    test(() => {
        expect_static_member('secureStaticMethod', (testObject) => {
            return testObject.secureStaticMethod();
          });
      }, 'Secure static method should exist and return value');

    test(() => {
        expect_member('secureAttributePartial', (testObject) => {
            return testObject.secureAttributePartial;
          });
      }, 'Secure attribute should exist on partial interface and return value');

    test(() => {
        expect_static_member('secureStaticAttributePartial', (testObject) => {
            return testObject.secureStaticAttributePartial;
          });
      }, 'Secure static attribute should exist on partial interface and return value');

    test(() => {
        expect_member('secureMethodPartial', (testObject) => {
            return testObject.secureMethodPartial();
          });
      }, 'Secure method should exist on partial interface and return value');

    test(() => {
        expect_static_member('secureStaticMethodPartial', (testObject) => {
            return testObject.secureStaticMethodPartial();
          });
      }, 'Secure static method should exist on partial interface and return value');
  }

};

// Verify that all IDL members are correctly exposed with an enabled trial, with
// an insecure context.
expect_success_bindings_insecure_context = () => {
  expect_success_bindings(true);
};

// Verify that all IDL members are not exposed with a disabled trial.
expect_failure_bindings_impl = (insecure_context, description_suffix) => {
  expect_always_bindings(insecure_context, description_suffix);

  test(() => {
      expect_member_fails('normalMethod');
    }, 'Method should not exist, with trial disabled');

  test(() => {
      expect_static_member_fails('staticMethod');
    }, 'Static method should not exist, with trial disabled');


  // Tests for combination of [OriginTrialEnabled] and [SecureContext]
  if (insecure_context) {
    // Origin trials only work in secure contexts, so tests cannot distinguish
    // between [OriginTrialEnabled] or [SecureContext] preventing exposure of
    // IDL members. There are tests to ensure IDL members are not exposed in
    // insecure contexts in expect_success_bindings().
    return;
  }
  test(() => {
      expect_member_fails('secureAttribute');
    }, 'Secure attribute should not exist, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticAttribute');
    }, 'Secure static attribute should not exist, with trial disabled');
  test(() => {
      expect_member_fails('secureMethod');
    }, 'Secure method should not exist, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticMethod');
    }, 'Secure static method should not exist, with trial disabled');
  test(() => {
      expect_member_fails('secureAttributePartial');
    }, 'Secure attribute should not exist on partial interface, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticAttributePartial');
    }, 'Secure static attribute should not exist on partial interface, with trial disabled');
  test(() => {
      expect_member_fails('secureMethodPartial');
    }, 'Secure method should not exist on partial interface, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticMethodPartial');
    }, 'Secure static method should not exist on partial interface, with trial disabled');
};

// Verify that all IDL members are not exposed with a disabled trial.
// Assumes a secure context.
expect_failure_bindings = (description_suffix) => {
  expect_failure_bindings_impl(false, description_suffix);
};

// Verify that all IDL members are not exposed with a disabled trial, with an
// insecure context
expect_failure_bindings_insecure_context = (description_suffix) => {
  expect_failure_bindings_impl(true, description_suffix);
};
