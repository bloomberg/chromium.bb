importScripts("/resources/testharness.js");

test(function() { assert_true("storage" in navigator); },
    "These worker tests require navigator.storage");

test(function() { assert_false("persist" in navigator.storage); },
    "navigator.storage.persist should not exist in workers");

promise_test(function() {
    var promise = navigator.storage.persisted();
    assert_true(promise instanceof Promise,
        "navigator.storage.persisted() returned a Promise.");
    return promise.then(function (result) {
        // Layout tests get canned results, not the value per spec. So testing
        // their values here would only be testing our test plumbing. But we can
        // test that the type of the returned value is correct.
        assert_equals(typeof result, "boolean", result + " should be a boolean");
    });
}, "navigator.storage.persisted returns a promise that resolves.");

done();
