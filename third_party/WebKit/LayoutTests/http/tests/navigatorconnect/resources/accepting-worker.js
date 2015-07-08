self.addEventListener('crossoriginconnect', function(event) {
  event.acceptConnection(true);
});

navigator.services.addEventListener('connect', function(event) {
  event.respondWith({accept: true});
});
