self.onmessage = function(e) {
  var port = e.data.port;
  var options = e.data.options;

  e.waitUntil(self.clients.matchAll(options).then(function(clients) {
      var message = [];
      clients.forEach(function(client) {
          message.push([client.visibilityState,
                        client.focused,
                        client.url,
                        client.frameType]);
        });
      port.postMessage(message);
    }));
};
