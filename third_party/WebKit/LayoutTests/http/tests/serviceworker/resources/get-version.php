<?php
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header('Content-Type:application/javascript');

echo "version = '" . microtime() . "';\n";
?>
