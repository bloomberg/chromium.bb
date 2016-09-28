<?php
    header("Suborigin: foobar");
?>
<!DOCTYPE html>
<head>
<meta charset="utf-8">
<title>Verify that thrown exception is sanitized when crossorigin script loaded without crossorigin attribute</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
setup({ allow_uncaught_exception: true });

window.onerror = function(msg, url, line, column, error) {
  assert_false(/SomeError/.test(msg));
  assert_equals(url, '');
  assert_equals(line, 0);
  assert_equals(column, 0);
  assert_equals(error, null);
  done();
}
</script>
<script src="/security/resources/cors-script.php?fail=true&cors=false"></script>
</body>
</html>
