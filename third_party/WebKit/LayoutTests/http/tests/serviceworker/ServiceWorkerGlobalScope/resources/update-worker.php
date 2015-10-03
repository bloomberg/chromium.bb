<?php
// update() does not bypass cache so set the max-age to 0 such that update()
// can find a new version in the network.
header('Cache-Control: max-age=0');

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
