<?php header("X-XSS-Protection: 1"); ?>
<!DOCTYPE html>
<html>
<head>
<script>alert(/This is a safe script./)</script>
</head>
<body>
</body>
</html>
