<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Block frame not in suborigin from accessing a frame in a suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
window.onmessage = function (event) {
  assert_equals(event.data, 'SecurityError: Blocked a frame with origin ' +
                            '\"http://127.0.0.1:8000\" from accessing a ' +
                            'cross-origin frame.');
  done();
};
</script>
<iframe src="resources/reach-into-iframe.php?childsuborigin=foobar"></iframe>
</html>
