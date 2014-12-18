self.addEventListener('crossoriginconnect', function(event) {
  var targetUrl = new URL(event.client.targetUrl);
  event.acceptConnection(new Promise(function(resolve, reject) {
      if (targetUrl.search == "?accept")
        self.setTimeout(resolve, 1, true);
      else
        self.setTimeout(reject, 1);
    }));
});
