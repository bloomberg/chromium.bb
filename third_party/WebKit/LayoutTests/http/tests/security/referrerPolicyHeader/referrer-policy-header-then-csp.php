<?php
header("Referrer-Policy: origin");
?>
<!DOCTYPE html>
<head>
  <meta http-equiv="Content-Security-Policy" content="script-src 'unsafe-inline' *; child-src *; referrer no-referrer">
  <script src="/resources/testharness.js"></script>
  <script src="/resources/testharnessreport.js"></script>
  <script src="/resources/get-host-info.js"></script>
</head>
<body>
</body>
<script>
    var policy = "no-referrer";
    var expectedReferrer = "";
    var navigateTo = "same-origin";
</script>
<script src="resources/header-test.js"></script>
</html>
