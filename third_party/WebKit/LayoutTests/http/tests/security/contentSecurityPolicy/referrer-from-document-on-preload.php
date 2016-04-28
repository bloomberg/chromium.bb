<?php
header("Content-Security-Policy: referrer origin-when-cross-origin");
?>
<html>
<head>
</head>
<img src="http://localhost:8000/security/resources/green-if-referrer-is-origin.php" />
</html>