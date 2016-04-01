<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Service Worker access fails from a suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
var expectedError = new DOMException("TEST EXCEPTION", "SecurityError");
assert_throws(expectedError, function() {
    navigator.serviceWorker;
});
done();
</script>
</html>
