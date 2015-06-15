<?php
// Force the browser to cache this script. update() should always bypass this
// cache and fetch a new version.
header('Cache-Control: max-age=86400');

// Return a different script for each access.
header('Content-Type:application/javascript');
echo '// ' . microtime()
?>
