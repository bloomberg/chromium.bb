self.onfetch = function(e) {
  if (e.request.url.indexOf('clients-get-frame.html') >= 0 ||
      e.request.url.indexOf('clients-get-client-types') >= 0) {
    // On navigation, the client id should be null.
    if (e.clientId === null) {
      e.respondWith(fetch(e.request));
    } else {
      e.respondWith(Response.error());
    }
    return;
  }
  e.respondWith(new Response(e.clientId));
};

self.onmessage = function(e) {
  var port = e.data.port;
  var client_ids = e.data.clientIds;
  var message = [];

  e.waitUntil(Promise.all(
      client_ids.map(function(client_id) {
          return self.clients.get(client_id);
        }))
      .then(function(clients) {
          // No matching client for a given id or a matched client is off-origin
          // from the service worker.
          if (clients.length == 1 && clients[0] == undefined) {
            port.postMessage(clients[0]);
          } else {
            clients.forEach(function(client) {
                if (client instanceof Client) {
                  message.push([client.visibilityState,
                                client.focused,
                                client.url,
                                client.frameType]);
                } else {
                  message.push(client);
                }
              });
            port.postMessage(message);
          }
        }));
};
