<?php
header("Content-Security-Policy-Report-Only: script-src 'unsafe-inline';");
?>
<!DOCTYPE html>
<html>
<head>
    <script src="resources/report-test.js"></script>
</head>
<body>
    <p>This test passes if a console message is present, warning about the missing 'report-uri' directive.</p>
</body>
</html>
