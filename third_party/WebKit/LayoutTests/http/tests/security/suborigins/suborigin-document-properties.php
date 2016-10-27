<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Validate behavior of document properties</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
assert_equals(document.domain, '127.0.0.1',
    'document.domain should not be affected by the suborigin.');
assert_equals(document.origin, 'http-so://foobar.127.0.0.1:8000',
    'document.origin should reflect the suborigin serialized in the origin.');
assert_equals(document.suborigin, 'foobar',
    'document.suborigin should reflect the suborigin name.');
done();
</script>
</html>
