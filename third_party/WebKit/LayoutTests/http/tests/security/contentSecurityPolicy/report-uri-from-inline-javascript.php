<?php
header("Content-Security-Policy: img-src 'none'; report-uri resources/save-report.php?test=report-uri-from-inline-javascript.php");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="resources/report-test.js"></script>
</head>
<body>
    <script>
        // This script block will trigger a violation report.
        var i = document.createElement('img');
        i.src = '/security/resources/abe.png';
        document.body.appendChild(i);
    </script>
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
