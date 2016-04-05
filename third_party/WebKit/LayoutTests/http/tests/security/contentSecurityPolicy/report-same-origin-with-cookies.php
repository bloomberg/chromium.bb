<?php
header("Content-Security-Policy: img-src 'none'; report-uri /security/contentSecurityPolicy/resources/save-report.php?test=report-same-origin-with-cookies.php");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="resources/report-test.js"></script>
</head>
<body>
<script>
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/cookies/resources/setCookies.cgi", false);
    xhr.setRequestHeader("SET-COOKIE", "cspViolationReportCookie=sameOrigin;path=/");
    xhr.send(null);
</script>

<!-- This image will generate a CSP violation report. -->
<img src="/security/resources/abe.png">

<script src='resources/go-to-echo-report.js'></script>
</body>
</html>
