self.addEventListener('message', function(event) {
    switch (event.data.type) {
      case 'ping':
        var results = [
            'Ping message: ' + event,
            '  event.origin: ' + event.origin,
            '  event.lastEventId: ' + event.lastEventId,
            '  event.source: ' + event.source,
            '  event.source.scriptURL: ' + event.source.scriptURL,
            '  event.source.state: ' + event.source.state,
            '  event.ports: ' + event.ports,
        ];
        var client_id = event.data.client_id;
        event.waitUntil(clients.get(client_id)
            .then(function(client) {
                client.postMessage({type: 'record', results: results});
                event.source.postMessage({type: 'pong', client_id: client_id});
              }));
        break;
    }
  });
