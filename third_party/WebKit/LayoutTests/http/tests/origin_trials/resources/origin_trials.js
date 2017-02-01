// The sample API integrates origin trial checks at various entry points.
// References to "partial interface" mean that the [OriginTrialEnabled]
// IDL attribute is applied to an entire partial interface, instead of
// applied to individual IDL members.

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
        assert_false('bindingsTest' in testObject);
        assert_not_exists(testObject, 'bindingsTest');
        assert_equals(testObject['bindingsTest'], undefined);
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
  }, 'Attribute should exist and return value');

test(() => {
    assert_idl_attribute(window.internals, 'originTrialsTest');
    var testObject = window.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'bindingsTest');
    assert_true(testObject.bindingsTest, 'Attribute should return boolean value');
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
    assert_idl_attribute(testObject, 'methodPartial');
    assert_true(testObject.methodPartial(), 'Method should return boolean value');
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
