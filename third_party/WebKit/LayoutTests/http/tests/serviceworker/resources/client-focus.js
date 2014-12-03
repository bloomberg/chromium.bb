self.onmessage = function(e) {
  self.clients.getAll().then(function(clients) {
    clients.forEach(function(client) {
      if (('focus' in client) && (typeof(client.focus) == 'function'))
        client.postMessage('focus() is present');
      client.focus().then(function(result) {
        client.postMessage('focus() succeeded with ' + result);
        client.postMessage('quit');
      });
    });
  });
}
