<?php
$fp = fopen("origin-signed-response-iframe.htxg", "rb");
header('Content-Type: application/http-exchange+cbor');
header("HTTP/1.0 200 OK");
fpassthru($fp);
?>
