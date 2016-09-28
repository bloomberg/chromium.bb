<?php
    header("Suborigin: foobar");
?>
<!DOCTYPE html>
<head>
<meta charset="utf-8">
<title>Verify that thrown exception is unsanitized when crossorigin script loaded with crossorigin attribute</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
setup({ allow_uncaught_exception: true });

window.onerror = function(msg, url, line, column, error) {
  assert_true(/SomeError/.test(msg));
  assert_equals(
    url,
    'http://127.0.0.1:8000/security/resources/cors-script.php?' +
    'fail=true&cors=http-so://foobar.127.0.0.1:8000')
  assert_equals(line, 1)
  assert_equals(column, 1);
  assert_not_equals(error, null);
  done();
}
</script>
<script crossorigin src="/security/resources/cors-script.php?fail=true&cors=http-so://foobar.127.0.0.1:8000"></script>
</body>
</html>
