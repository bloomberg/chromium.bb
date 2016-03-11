self.addEventListener('message', function(event) {
    var results = [
        'Client message: ' + event,
        '  event.origin: ' + event.origin,
        '  event.lastEventId: ' + event.lastEventId,
        '  event.source: ' + event.source,
        '  event.source.url: ' + event.source.url,
        '  event.source.frameType: ' + event.source.frameType,
        '  event.source.visibilityState: ' + event.source.visibilityState,
        '  event.source.focused: ' + event.source.focused,
        '  event.ports: ' + event.ports,
    ];
    event.source.postMessage(results);
  });
