<?php
$origin = "http://127.0.0.1:8000";
if ($_GET['origin'])
    $origin = $_GET['origin'];

header("HTTP/1.1 307 Moved Temporarily");
header("Location: ${origin}/credentialmanager/echo-post.php");
?>
