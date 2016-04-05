<?php
header("Content-Security-Policy: default-src 'self'; report-uri ../resources/save-report.php?test=report-uri-effective-directive.php");
?>
<!DOCTYPE html>
<html>
<head>
</head>
<body>
    <script>
        // This script block will trigger a violation report.
        alert('FAIL');
    </script>
    <script src="../resources/go-to-echo-report.js"></script>
</body>
</html>
