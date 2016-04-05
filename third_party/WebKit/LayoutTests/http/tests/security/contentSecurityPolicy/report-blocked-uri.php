<?php
header("Content-Security-Policy-Report-Only: img-src 'none'; report-uri resources/save-report.php?test=report-blocked-uri.php");
?>
<script src="resources/report-test.js"></script>
The URI of this image should show up in the violation report.
<img src="../resources/abe.png#the-fragment-should-not-be-in-report">
<script src="resources/go-to-echo-report.js"></script>
