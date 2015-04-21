<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Validate presence and value of document.suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
assert_equals(document.suborigin, "foobar", "document.suborigin should exist when page has a suborigin.");
done();
</script>
</html>
