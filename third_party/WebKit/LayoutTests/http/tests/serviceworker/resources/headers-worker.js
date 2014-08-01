importScripts('worker-test-harness.js');

test(function() {
    var expectedMap = {
        'content-language': 'ja',
        'content-type': 'text/html; charset=UTF-8',
        'x-serviceworker-test': 'response test field'
    };

    var headers = new Headers;
    headers.set('Content-Language', 'ja');
    headers.set('Content-Type', 'text/html; charset=UTF-8');
    headers.set('X-ServiceWorker-Test', 'text/html; charset=UTF-8');

    // 'size'
    assert_equals(headers.size, 3, 'headers.size should match');

    // 'has()', 'get()'
    var key = 'Content-Type';
    assert_true(headers.has(key));
    assert_true(headers.has(key.toUpperCase()));
    assert_equals(headers.get(key), expectedMap[key.toLowerCase()]);
    assert_equals(headers.get(key.toUpperCase()), expectedMap[key.toLowerCase()]);
    assert_equals(headers.get('dummy'), null);
    assert_false(headers.has('dummy'));

    // 'delete()'
    var deleteKey = 'Content-Type';
    headers.delete(deleteKey);
    assert_equals(headers.size, 2, 'headers.size should have -1 size');
    Object.keys(expectedMap).forEach(function(key) {
        if (key == deleteKey.toLowerCase())
            assert_false(headers.has(key));
        else
            assert_true(headers.has(key));
    });

    // 'set()'
    var testCasesForSet = [
        // For a new key/value pair.
        { key: 'Cache-Control',
          value: 'max-age=3600',
          isNewEntry: true },

        // For an existing key.
        { key: 'X-ServiceWorker-Test',
          value: 'response test field - updated',
          isUpdate: true },

        // For setting a numeric value, expecting to see DOMString on getting.
        { key: 'X-Numeric-Value',
          value: 12345,
          expectedValue: '12345',
          isNewEntry: true },

        // For case-insensitivity test.
        { key: 'content-language',
          value: 'fi',
          isUpdate: true }
    ];

    var expectedHeaderSize = headers.size;
    testCasesForSet.forEach(function(testCase) {
        var key = testCase.key;
        var value = testCase.value;
        var expectedValue = ('expectedValue' in testCase) ? testCase.expectedValue : testCase.value;
        expectedHeaderSize = testCase.isNewEntry ? (expectedHeaderSize + 1) : expectedHeaderSize;

        headers.set(key, value);
        assert_true(headers.has(key));
        assert_equals(headers.get(key), expectedValue);
        if (testCase.isUpdate)
            assert_true(headers.get(key) != expectedMap[key.toLowerCase()]);
        assert_equals(headers.size, expectedHeaderSize);

        // Update expectedMap too for forEach() test below.
        expectedMap[key.toLowerCase()] = expectedValue;
    });

    // 'forEach()'
    headers.forEach(function(value, key) {
        assert_true(key != deleteKey.toLowerCase());
        assert_true(key in expectedMap);
        assert_equals(headers.get(key), expectedMap[key]);
    });

    // 'append()', 'getAll()'
    var allValues = headers.getAll('X-ServiceWorker-Test');
    assert_equals(allValues.length, 1);
    assert_equals(headers.size, 4);
    headers.append('X-SERVICEWORKER-TEST', 'response test field - append');
    assert_equals(headers.size, 5, 'headers.size should increase by 1.');
    assert_equals(headers.get('X-SERVICEWORKER-Test'),
                  'response test field - updated',
                  'the value of the first header added should be returned.');
    allValues = headers.getAll('X-SERVICEWorker-TEST');
    assert_equals(allValues.length, 2);
    assert_equals(allValues[0], 'response test field - updated');
    assert_equals(allValues[1], 'response test field - append');
    headers.set('X-SERVICEWorker-Test', 'response test field - set');
    assert_equals(headers.size, 4, 'the second header should be deleted');
    allValues = headers.getAll('X-ServiceWorker-Test');
    assert_equals(allValues.length, 1, 'the second header should be deleted');
    assert_equals(allValues[0], 'response test field - set');
    headers.append('X-ServiceWorker-TEST', 'response test field - append');
    assert_equals(headers.size, 5, 'headers.size should increase by 1.')
    headers.delete('X-ServiceWORKER-Test');
    assert_equals(headers.size, 3, 'two headers should be deleted.')

    // new Headers with sequence<sequence<ByteString>>
    headers = new Headers([['a', 'b'], ['c', 'd'], ['c', 'e']]);
    assert_equals(headers.size, 3, 'headers.size should match');
    assert_equals(headers.get('a'), 'b');
    assert_equals(headers.get('c'), 'd');
    assert_equals(headers.getAll('c')[0], 'd');
    assert_equals(headers.getAll('c')[1], 'e');

    // new Headers with Headers
    var headers2 = new Headers(headers);
    assert_equals(headers2.size, 3, 'headers.size should match');
    assert_equals(headers2.get('a'), 'b');
    assert_equals(headers2.get('c'), 'd');
    assert_equals(headers2.getAll('c')[0], 'd');
    assert_equals(headers2.getAll('c')[1], 'e');
    headers.set('a', 'x');
    assert_equals(headers.get('a'), 'x');
    assert_equals(headers2.get('a'), 'b');

    // new Headers with Dictionary
    headers = new Headers({'a': 'b', 'c': 'd'});
    assert_equals(headers.size, 2, 'headers.size should match');
    assert_equals(headers.get('a'), 'b');
    assert_equals(headers.get('c'), 'd');

    // Throw errors
    var invalidNames = ['', '(', ')', '<', '>', '@', ',', ';', ':', '\\', '"',
                        '/', '[', ']', '?', '=', '{', '}', '\u3042', 'a(b'];
    invalidNames.forEach(function(name) {
        assert_throws({name:'TypeError'},
                      function() { headers.append(name, 'a'); },
                      'Headers.append with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() { headers.delete(name); },
                      'Headers.delete with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() { headers.get(name); },
                      'Headers.get with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() { headers.getAll(name); },
                      'Headers.getAll with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() { headers.has(name); },
                      'Headers.has with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() { headers.set(name, 'a'); },
                      'Headers.set with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() {
                        var obj = {};
                        obj[name] = 'a';
                        var headers = new Headers(obj);
                      },
                      'new Headers with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() { var headers = new Headers([[name, 'a']]); },
                      'new Headers with an invalid name (' + name +') should throw');
    });

    var invalidValues = ['test \r data', 'test \n data'];
    invalidValues.forEach(function(value) {
        assert_throws({name:'TypeError'},
                      function() { headers.append('a', value); },
                      'Headers.append with an invalid value should throw');
        assert_throws({name:'TypeError'},
                      function() { headers.set('a', value); },
                      'Headers.set with an invalid value should throw');
        assert_throws({name:'TypeError'},
                      function() { var headers = new Headers({'a': value}); },
                      'new Headers with an invalid value should throw');
        assert_throws({name:'TypeError'},
                      function() { var headers = new Headers([['a', value]]); },
                      'new Headers with an invalid value should throw');
    });

    assert_throws({name:'TypeError'},
                  function() { var headers = new Headers([[]]); },
                  'new Headers with a sequence with less than two strings should throw');
    assert_throws({name:'TypeError'},
                  function() { var headers = new Headers([['a']]); },
                  'new Headers with a sequence with less than two strings should throw');
    assert_throws({name:'TypeError'},
                  function() { var headers = new Headers([['a', 'b'], []]); },
                  'new Headers with a sequence with less than two strings should throw');
    assert_throws({name:'TypeError'},
                  function() { var headers = new Headers([['a', 'b'], ['x', 'y', 'z']]); },
                  'new Headers with a sequence with more than two strings should throw');

    // 'forEach()' with thisArg
    var that = {}, saw_that;
    headers = new Headers;
    headers.set('a', 'b');
    headers.forEach(function() { saw_that = this; }, that);
    assert_equals(saw_that, that, 'Passed thisArg should match');

    headers.forEach(function() { 'use strict'; saw_that = this; }, 'abc');
    assert_equals(saw_that, 'abc', 'Passed non-object thisArg should match');

    headers.forEach(function() { saw_that = this; }, null);
    assert_equals(saw_that, self, 'Passed null thisArg should be replaced with global object');
}, 'Headers in ServiceWorkerGlobalScope');
