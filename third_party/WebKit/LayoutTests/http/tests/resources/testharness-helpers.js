/*
 * testharness-helpers contains various useful extensions to testharness.js to
 * allow them to be used across multiple tests before they have been
 * upstreamed. This file is intended to be usable from both document and worker
 * environments, so code should for example not rely on the DOM.
 */

// A testharness test that simplifies testing with promises.
//
// * The |func| argument should be a reference to a function that optionally
//   takes a test object as an argument and returns a Promise (or really any
//   thenable object).
//
// * Resolution of the promise completes the test. A rejection causes the test
//   to fail. The test harness waits for the promise to resolve.
//
// * Assertions can be made at any point in the promise handlers. Assertions
//   only need to be wrapped in test.step_func()s if the promise chain contains
//   rejection handlers.
//
// E.g.:
//   promise_test(function(t) {
//       return method_that_returns_a_promise()
//         .then(function(result) {
//             assert_equals(result, expected_result, "Should be expected.");
//           });
//     }, 'Promise based test');
function promise_test(func, name, properties) {
  properties = properties || {};
  var test = async_test(name, properties);
  Promise.resolve(test.step(func, test, test))
    .then(function() { test.done(); })
    .catch(test.step_func(function(value) {
        throw value;
      }));
}

// Returns a promise that fulfills after the provided |promise| is fulfilled.
// The |test| succeeds only if |promise| rejects with an exception matching
// |code|. Accepted values for |code| follow those accepted for assert_throws().
// The optional |description| describes the test being performed.
//
// E.g.:
//   assert_promise_rejects(
//       new Promise(...), // something that should throw an exception.
//       'NotFoundError',
//       'Should throw NotFoundError.');
//
//   assert_promise_rejects(
//       new Promise(...),
//       new TypeError(),
//       'Should throw TypeError');
function assert_promise_rejects(promise, code, description) {
  return promise.then(
    function() {
      throw 'assert_promise_rejects: ' + description + ' Promise did not reject.';
    },
    function(e) {
      if (code !== undefined) {
        assert_throws(code, function() { throw e; }, description);
      }
    });
}

// Asserts that two objects |actual| and |expected| are weakly equal under the
// following definition:
//
// |a| and |b| are weakly equal if any of the following are true:
//   1. If |a| is not an 'object', and |a| === |b|.
//   2. If |a| is an 'object', and all of the following are true:
//     2.1 |a.p| is weakly equal to |b.p| for all own properties |p| of |a|.
//     2.2 Every own property of |b| is an own property of |a|.
//
// This is a replacement for the the version of assert_object_equals() in
// testharness.js. The latter doesn't handle own properties correctly. I.e. if
// |a.p| is not an own property, it still requires that |b.p| be an own
// property.
//
// Note that |actual| must not contain cyclic references.
self.assert_object_equals = function(actual, expected, description) {
  var object_stack = [];

  function _is_equal(actual, expected, prefix) {
    if (typeof actual !== 'object') {
      assert_equals(actual, expected, prefix);
      return;
    }
    assert_true(typeof expected === 'object', prefix);
    assert_equals(object_stack.indexOf(actual), -1,
                  prefix + ' must not contain cyclic references.');

    object_stack.push(actual);

    Object.getOwnPropertyNames(expected).forEach(function(property) {
        assert_own_property(actual, property, prefix);
        _is_equal(actual[property], expected[property],
                  prefix + '.' + property);
      });
    Object.getOwnPropertyNames(actual).forEach(function(property) {
        assert_own_property(expected, property, prefix);
      });

    object_stack.pop();
  }

  function _brand(object) {
    return Object.prototype.toString.call(object).match(/^\[object (.*)\]$/)[1];
  }

  _is_equal(actual, expected,
            (description ? description + ': ' : '') + _brand(actual));
};

// Equivalent to assert_in_array, but uses a weaker equivalence relation
// (assert_object_equals) than '==='.
function assert_object_in_array(actual, expected_array, description) {
  assert_true(expected_array.some(function(element) {
      try {
        assert_object_equals(actual, element);
        return true;
      } catch (e) {
        return false;
      }
    }), description);
}

// Assert that the two arrays |actual| and |expected| contain the same set of
// elements as determined by assert_object_equals. The order is not significant.
//
// |expected| is assumed to not contain any duplicates as determined by
// assert_object_equals().
function assert_array_equivalent(actual, expected, description) {
  assert_true(Array.isArray(actual), description);
  assert_equals(actual.length, expected.length, description);
  expected.forEach(function(expected_element) {
      // assert_in_array treats the first argument as being 'actual', and the
      // second as being 'expected array'. We are switching them around because
      // we want to be resilient against the |actual| array containing
      // duplicates.
      assert_object_in_array(expected_element, actual, description);
    });
}

// Asserts that two arrays |actual| and |expected| contain the same set of
// elements as determined by assert_object_equals(). The corresponding elements
// must occupy corresponding indices in their respective arrays.
function assert_array_objects_equals(actual, expected, description) {
  assert_true(Array.isArray(actual), description);
  assert_equals(actual.length, expected.length, description);
  actual.forEach(function(value, index) {
      assert_object_equals(value, expected[index],
                           description + ' : object[' + index + ']');
    });
}
