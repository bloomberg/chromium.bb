<?php
// Force the browser to cache this script. update() should always bypass this
// cache and fetch a new version.
header('Cache-Control: max-age=86400');

// Return a different script for each access.
header('Content-Type:application/javascript');
echo '// ' . microtime();
?>

importScripts('../../resources/test-helpers.js');
importScripts('../../resources/worker-testharness.js');

var events_seen = [];

self.registration.addEventListener('updatefound', function() {
    events_seen.push('updatefound');
  });

self.addEventListener('activate', function(e) {
    events_seen.push('activate');
  });

self.addEventListener('fetch', function(e) {
    events_seen.push('fetch');
    e.respondWith(new Response(events_seen));
  });

self.addEventListener('message', function(e) {
    events_seen.push('message');
    self.registration.update();
  });

// update() during the script evaluation should be ignored.
self.registration.update();
