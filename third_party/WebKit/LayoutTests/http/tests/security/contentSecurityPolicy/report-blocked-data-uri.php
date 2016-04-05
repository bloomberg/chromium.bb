<?php
header("Content-Security-Policy: img-src 'none'; report-uri resources/save-report.php?test=report-blocked-data-uri.php");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="resources/report-test.js"></script>
</head>
<body>
    <img src="data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==">
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
