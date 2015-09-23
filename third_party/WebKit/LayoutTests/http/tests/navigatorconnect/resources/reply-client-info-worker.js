var client;
self.addEventListener('crossoriginmessage', function(event) {
  client.postMessage({origin: event.origin});
});

navigator.services.addEventListener('connect', function(event) {
  event.respondWith({accept: true, data: {origin: event.origin}})
    .then(function(port) {
        client = port;
        client.postMessage({origin: event.origin, targetUrl: event.targetURL});
      });
});
