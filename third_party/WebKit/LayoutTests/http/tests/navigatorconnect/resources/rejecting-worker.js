self.addEventListener('crossoriginconnect', function(event) {
  event.acceptConnection(false);
});

navigator.services.addEventListener('connect', function(event) {
  event.respondWith({accept: false});
});
