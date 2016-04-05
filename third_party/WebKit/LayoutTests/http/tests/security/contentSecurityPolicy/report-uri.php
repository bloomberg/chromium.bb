<?php
header("Content-Security-Policy: script-src 'self'; report-uri resources/save-report.php?test=report-uri.php");
?>
<script src="resources/report-test.js"></script>
<script>
// This script block will trigger a violation report.
alert('FAIL');
</script>
<script src="resources/go-to-echo-report.js"></script>
