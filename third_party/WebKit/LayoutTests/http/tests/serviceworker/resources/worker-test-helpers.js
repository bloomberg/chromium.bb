// Subset of testharness.js API for use in Service Worker tests.
// FIXME: Support async tests, possibly by just pulling in testharness.js

// Example:
//   test(function() {
//     assert_true(n > 1, 'n should be greater than 1');
//     assert_false(n < 1, 'n should not be less than 1');
//     assert_equals(n, 1, 'n should be equal to 1');
//     assert_array_equals(a, [1,2,3], 'arrays should match');
//     assert_throws({name: 'TypeError'}, function() { "123".call(); }, 'Should throw');
//     if (n < 0) {
//       assert_unreached('This should not happen');
//     }
//   }, 'Test description here');

(function(global) {

    function assert(expected_true, error, description) {
        if (!expected_true) {
            var message = description ? error + ': ' + description : error;
            throw message;
        }
    }

    function assert_true(actual, m) {
        assert(actual === true, 'expected true, got ' + actual, m);
    }

    function assert_false(actual, m) {
        assert(actual === false, 'expected false, got ' + actual, m);
    }


    // Same as Object.is() from ES6
    function is(a, b) {
        if (a === b)
            return (a !== 0) || (1 / a === 1 / b); // -0
        return a !== a && b !== b; // NaN
    }
    assert(is(NaN, NaN), 'NaN should be equal to itself');
    assert(!is(0, -0), '0 and -0 should not be equal');

    function assert_equals(actual, expected, m) {
        assert(is(actual, expected),
               'expected (' + typeof expected + ') ' + expected +
               ' but got (' + typeof actual + ') ' + actual, m);
    }

    function assert_array_equals(actual, expected, m) {
        assert(actual.length === expected.length,
               'lengths differ, expected ' + expected.length + ' got ' + actual.length, m);
        for (var i = 0; i < expected.length; ++i) {
            var ai = actual[i], ei = expected[i];
            assert(is(ai, ei),
                   'index ' + i + ', expected (' + typeof ei + ') ' + ei +
                   ' but got (' + typeof ai + ') ' + ai, m);
        }
    }

    function assert_throws(example, func, m) {
        try {
            func();
        } catch (ex) {
            if (example)
                assert(ex.name === example.name,
                       String(func) + ' threw ' + ex.name + ' expected ' + example.name, m);
            return;
        }
        assert(false, String(func) + ' did not throw', m);
    }

    function assert_unreached(m) {
        assert(false, 'Reached unreachable code', m);
    }

    function test(func, name) {
        tests.push({func:func, name:name});
    }

    // This assumes the test is started via 'service_worker_test' in test-helpers.js
    var tests = [];
    self.addEventListener('message', function(e) {
        if (e.ports.length) {
            var port = e.ports[0];
            try {
                tests.forEach(function(test) { test.func(); });
                port.postMessage('pass');
            } catch (ex) {
                port.postMessage(ex);
            }
        }
    });

    // Exports
    global.assert_true = assert_true;
    global.assert_false = assert_false;
    global.assert_equals = assert_equals;
    global.assert_array_equals = assert_array_equals;
    global.assert_throws = assert_throws;
    global.assert_unreached = assert_unreached;
    global.test = test;

}(self));
