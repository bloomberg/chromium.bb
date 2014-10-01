importScripts('worker-test-harness.js');

this.addEventListener('fetch', function(event) {
    event.respondWith(new Response('ERROR'));
  });