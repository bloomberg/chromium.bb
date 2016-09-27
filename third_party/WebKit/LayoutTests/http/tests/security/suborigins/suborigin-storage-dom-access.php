<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Verifies that localStorage and sessionStorage are not accessible from within a suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var expected_error = new DOMException('TEST EXCEPTION', 'SecurityError');

function must_throw_security_exception(storage) {
  return function(t) {
    assert_throws(expected_error, function() {
        window[storage];
      });
    t.done();
  };
}

async_test(must_throw_security_exception('localStorage'),
  'localStorage must not be accessible from a suborigin');
async_test(must_throw_security_exception('sessionStorage'),
  'sessionStorage must not be accessible from a suborigin');
</script>
</body>
</html>
