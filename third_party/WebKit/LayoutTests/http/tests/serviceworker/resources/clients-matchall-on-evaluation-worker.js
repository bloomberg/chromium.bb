importScripts('test-helpers.js');

var page_url = normalizeURL('../clients-matchall-on-evaluation.html');

self.clients.matchAll({includeUncontrolled: true})
  .then(function(clients) {
      clients.forEach(function(client) {
          if (client.url == page_url)
            client.postMessage('matched');
        });
    });
