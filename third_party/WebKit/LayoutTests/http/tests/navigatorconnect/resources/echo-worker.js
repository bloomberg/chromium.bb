var client;
self.addEventListener('crossoriginconnect', function(event) {
  client = event.client;
  event.acceptConnection(true);
});

self.addEventListener('crossoriginmessage', function(event) {
  client.postMessage(event.data, event.ports);
});

navigator.services.addEventListener('connect', function(event) {
  event.respondWith({accept: true}).then(function(port) { client = port; });
});
