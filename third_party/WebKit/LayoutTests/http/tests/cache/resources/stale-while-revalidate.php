<?php
$last_modified = gmdate(DATE_RFC1123, time() - 1);

header('Cache-Control: private, max-age=0, stale-while-revalidate=1');
header('Last-Modified: ' . $last_modified);
header('Content-Type: application/javascript');
echo('report("' . $_SERVER['HTTP_RESOURCE_FRESHNESS'] . '");');
?>
