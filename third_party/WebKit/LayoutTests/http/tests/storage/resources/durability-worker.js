importScripts("/resources/testharness.js");

test(function() { assert_true("storage" in navigator); },
    "These worker tests require navigator.storage");

test(function() { assert_false("requestPersistent" in navigator.storage); },
    "navigator.storage.requestPersistent should not exist in workers");

promise_test(function() {
    var promise = navigator.storage.persistentPermission();
    assert_true(promise instanceof Promise,
        "navigator.storage.persistentPermission() returned a Promise.")
    return promise.then(function (result) {
        // Layout tests get canned results, not the value per spec. So testing
        // their values here would only be testing our test plumbing. But we can
        // test that the type of the returned value is correct.
        assert_equals(typeof result, "string", result + " should be a string");
        assert_greater_than(result.length, 0, "result should have length >0");
    });
}, "navigator.storage.persistentPermission returns a promise that resolves.");

done();
