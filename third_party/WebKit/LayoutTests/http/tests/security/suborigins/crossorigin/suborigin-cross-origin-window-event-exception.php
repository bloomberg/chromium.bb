<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Crossorigin access to window.event should throw a SecurityError</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var iframe = document.createElement('iframe');
iframe.src = 'http://127.0.0.1:8000/';
iframe.onload = function() {
  assert_throws('SecurityError', function() {
      var e = iframe.contentWindow.event;
    });
  assert_throws('SecurityError', function() {
      iframe.contentWindow.event = 1;
    });
  done();
};
document.body.appendChild(iframe);
</script>
</body>
</html>
