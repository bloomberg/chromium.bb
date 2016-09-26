<?php header("Suborigin: foobar"); ?>
<!DOCTYPE html>
<html>
<head>
<title>WebSocket access is disallowed in suborigins</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var expectedError = new DOMException("TEST EXCEPTION", "SecurityError");
assert_throws(expectedError, function() {
    var ws = new WebSocket("ws://127.0.0.1:8880/simple");
});
done();
</script>
</body>
</html>
