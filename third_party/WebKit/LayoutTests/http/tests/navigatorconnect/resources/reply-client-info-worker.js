var client;
self.addEventListener('crossoriginconnect', function(event) {
  client = event.client;
  event.acceptConnection(true);
  client.postMessage({origin: client.origin, targetUrl: client.targetUrl});
});


self.addEventListener('crossoriginmessage', function(event) {
  client.postMessage({origin: event.origin});
});
