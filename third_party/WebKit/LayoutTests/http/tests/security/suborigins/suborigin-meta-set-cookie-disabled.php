<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>&lt;meta http-equiv=Set-Cookie&gt; should not work in suborigin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

<meta http-equiv="Set-Cookie" content="TestCookieMetaSetCookieDisabledInSuborigin=a_value"></meta>
</head>
<body>
<script>
window.onmessage = function(e) {
  assert_equals(e.data.cookie_val, '');
  done();
};
</script>
<iframe src="resources/post-document-cookie.php"></iframe>
</body>
</html>
