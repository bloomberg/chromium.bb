<?php
header("Content-Security-Policy: img-src http://*");
header("Content-Security-Policy-Report-Only: img-src http://*; script-src 'self'; report-uri resources/save-report.php?test=report-uri-multiple.php");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="resources/report-test.js"></script>
</head>
<body>
    <img src="ftp://blah.test" />
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
