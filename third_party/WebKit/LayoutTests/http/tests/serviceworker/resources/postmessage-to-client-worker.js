self.onmessage = function(e) {
    self.clients.getServiced().then(function(clients) {
        clients.forEach(function(client) {
            client.postMessage('Sending message via clients');
            client.postMessage('quit');
        });
    });
};
