<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Crossorigin access to window.open and window.opener should throw a SecurityError</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var iframe = document.createElement('iframe');
iframe.src = '/';
iframe.onload = function() {
  assert_throws('SecurityError', function() {
      iframe.contentWindow.open();
    });
  assert_throws('SecurityError', function() {
      iframe.contentWindow.opener = 1;
    });
  done();
};
document.body.appendChild(iframe);
</script>
</body>
</html>
