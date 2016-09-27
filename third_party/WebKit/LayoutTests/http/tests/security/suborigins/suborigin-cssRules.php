<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Suborigin can't access cross origin cssRules</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<link rel="stylesheet" href="/security/resources/cssStyle.css"></link>
</head>
<script>
window.onload = function() {
  var sheet = document.styleSheets[0];
  assert_equals(sheet.cssRules, null,
    'stylesheet rules should not be readable from a suborigin');
  done();
};
</script>
</html>
