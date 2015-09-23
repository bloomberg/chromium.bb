<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Validate behavior of document.domain</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
assert_equals(document.domain, "127.0.0.1", "document.domain should not be affected by the suborigin.");
done();
</script>
</html>
