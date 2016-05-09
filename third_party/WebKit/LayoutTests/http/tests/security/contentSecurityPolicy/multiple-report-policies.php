<?php
header("Content-Security-Policy-Report-Only: img-src http://* https://*; default-src 'self'; report-uri resources/save-report.php?test=multiple-report-policies-1, img-src http://*; default-src 'self'; report-uri resources/save-report.php?test=multiple-report-policies-2");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="resources/report-test.js"></script>
</head>
<body>
    <img src="ftp://blah.test" />
    <iframe src="/security/contentSecurityPolicy/resources/echo-report.php?test=multiple-report-policies-1"></iframe>
    <iframe src="/security/contentSecurityPolicy/resources/echo-report.php?test=multiple-report-policies-2"></iframe>
</body>
</html>