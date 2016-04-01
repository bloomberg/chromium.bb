<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Verifies that localStorage and sessionStorage are not accessible from within a suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var expectedError = new DOMException("TEST EXCEPTION", "SecurityError");
var localStorageTest = async_test("localStorage must not be accessible from a suborigin");
var sessionStorageTest = async_test("sessionStorage must not be accessible from a suborigin");

function mustThrowSecurityException() {
    assert_throws(expectedError, function() {
        window.localStorage;
    });
    this.done();
}

localStorageTest.step(mustThrowSecurityException);
sessionStorageTest.step(mustThrowSecurityException);
</script>
</body>
</html>
