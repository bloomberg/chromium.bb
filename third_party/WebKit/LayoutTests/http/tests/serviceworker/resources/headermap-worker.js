importScripts('worker-test-helpers.js');

test(function() {
    var expectedMap = {
        'Content-Language': 'ja',
        'Content-Type': 'text/html; charset=UTF-8',
        'X-ServiceWorker-Test': 'response test field'
    };

    var headers = new HeaderMap;
    Object.keys(expectedMap).forEach(function(key) {
        headers.set(key, expectedMap[key]);
    });

    // 'size'
    assert_equals(headers.size, 3, 'headers.size should match');

    // 'has()', 'get()'
    var key = 'Content-Type';
    assert_true(headers.has(key));
    assert_equals(headers.get(key), expectedMap[key]);

    // 'delete()'
    var deleteKey = 'Content-Type';
    headers.delete(deleteKey);
    assert_equals(headers.size, 2, 'headers.size should have -1 size');
    Object.keys(expectedMap).forEach(function(key) {
        if (key == deleteKey)
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

        // For case-sensitivity test. (FIXME: if we want HeaderMap to
        // work in an case-insensitive way (as we do for headers in XHR)
        // update the test and code)
        { key: 'content-language',
          value: 'fi',
          isNewEntry: true }
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
            assert_true(headers.get(key) != expectedMap[key]);
        assert_equals(headers.size, expectedHeaderSize);

        // Update expectedMap too for forEach() test below.
        expectedMap[key] = expectedValue;
    });

    // 'forEach()'
    headers.forEach(function(value, key) {
        assert_true(key != deleteKey);
        assert_true(key in expectedMap);
        assert_equals(headers.get(key), expectedMap[key]);
    });

}, 'HeaderMap in ServiceWorkerGlobalScope');

