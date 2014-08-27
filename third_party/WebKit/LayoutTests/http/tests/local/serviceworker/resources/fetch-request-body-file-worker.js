self.addEventListener('fetch', function(event) {
    event.respondWith(new Promise(function(resolve) {
        var headers = [];
        event.request.headers.forEach(function(value, key) {
            headers.push([key, value]);
          });
        event.request.body.asText()
          .then(function(result) {
              resolve(new Response(JSON.stringify({
                  method: event.request.method,
                  headers: headers,
                  body: result
                })));
            })
      }));
  });
