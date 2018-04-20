<?php
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header('Content-Type:application/javascript');

echo '// ', microtime();
?>

// This no-op fetch handler is necessary to bypass explicitly the no fetch
// handler optimization by which this service worker script can be skipped.
addEventListener('fetch', event => {
    return;
  });

addEventListener('install', event => {
    event.waitUntil(self.skipWaiting());
  });

addEventListener('activate', event => {
    event.waitUntil(self.clients.claim());
  });
