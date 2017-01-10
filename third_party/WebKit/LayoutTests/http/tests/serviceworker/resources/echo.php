<?php
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header('Content-Type: application/javascript');

echo "echo_output = '" . $_SERVER['QUERY_STRING'] . "';\n";
?>
