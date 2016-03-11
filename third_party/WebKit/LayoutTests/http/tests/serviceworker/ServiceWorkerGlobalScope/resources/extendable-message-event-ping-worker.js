self.addEventListener('message', function(event) {
    switch (event.data.type) {
      case 'start':
        // Send a ping message to another service worker.
        self.registration.waiting.postMessage(
            {type: 'ping', client_id: event.source.id});
        break;
      case 'pong':
        var results = [
            'Pong message: ' + event,
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
                client.postMessage({type: 'finish'});
              }));
        break;
    }
  });
