<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
</head>
<body>
<iframe src="/"></iframe>
<script src="/js-test-resources/js-test.js"></script>
<script>
window.jsTestIsAsync = true; 
description(
  'Cross-origin access to \'window.open\' and \'window.opener\' should ' +
  'throw a SecurityError.');
var frame = document.querySelector('iframe');
window.onload = function () {
  shouldThrow(
    'frame.contentWindow.open()',
    '"SecurityError: Blocked a frame with origin ' +
    '\\"http-so://foobar.127.0.0.1:8000\\" from accessing a ' +
    'cross-origin frame."');
  shouldThrow(
    'frame.contentWindow.opener = 1;',
    '"SecurityError: Failed to set the \'opener\' property on ' +
    '\'Window\': Blocked a frame with origin ' +
    '\\"http-so://foobar.127.0.0.1:8000\\" from accessing a ' +
    'cross-origin frame."');
  finishJSTest();
};
</script>
</body>
</html>
