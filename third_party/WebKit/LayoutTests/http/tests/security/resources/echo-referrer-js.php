<?php
$url = $_SERVER['REQUEST_URI'];
$referrer = '[no-referrer]';
if (isset($_SERVER['HTTP_REFERER'])) {
    $referrer = $_SERVER['HTTP_REFERER'];
}
header('Access-Control-Allow-Origin: *');
header('Content-Type: application/javascript');
print 'console.log("url = ' . $url . ', referrer = ' . $referrer . '");';
?>
