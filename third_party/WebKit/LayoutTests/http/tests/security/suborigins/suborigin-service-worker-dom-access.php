<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Service worker JavaScript API fails from a suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
var expected_error = new DOMException('TEST EXCEPTION', 'SecurityError');
assert_throws(expected_error, function() {
    navigator.serviceWorker;
  });
done();
</script>
</html>
