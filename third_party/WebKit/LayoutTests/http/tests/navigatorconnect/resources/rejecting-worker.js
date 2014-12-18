self.addEventListener('crossoriginconnect', function(event) {
  event.acceptConnection(false);
});
