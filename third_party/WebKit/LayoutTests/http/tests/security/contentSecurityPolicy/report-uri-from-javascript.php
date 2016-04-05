<?php
header("Content-Security-Policy: img-src 'none'; report-uri resources/save-report.php?test=report-uri-from-javascript.php");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="resources/report-test.js"></script>
</head>
<body>
    <script src="resources/inject-image.js"></script>
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
