self.onmessage = function(e) {
  var port = e.data.port;

  self.clients.getAll().then(function(clients) {
      var message = [];
      clients.forEach(function(client) {
          message.push([client.visibilityState,
                        client.focused,
                        client.url,
                        client.frameType]);
        });
      // Sort by url
      message.sort(function(a, b) { return a[2] > b[2] ? 1 : -1; });
      port.postMessage(message);
    });
};
