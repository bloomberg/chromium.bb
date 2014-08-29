self.addEventListener('fetch', function(event) {
    event.respondWith(new Promise(function(resolve) {
        var headers = [];
        event.request.headers.forEach(function(value, key) {
            headers.push([key, value]);
          });
        if (event.request.url.indexOf('asText') != -1) {
          event.request.body.asText()
            .then(function(result) {
                resolve(new Response(JSON.stringify({
                    method: event.request.method,
                    headers: headers,
                    body: result
                  })));
              })
        } else if (event.request.url.indexOf('asBlob') != -1) {
          event.request.body.asBlob()
            .then(function(result) {
                resolve(new Response(JSON.stringify({
                    method: event.request.method,
                    headers: headers,
                    body_size: result.size
                  })));
              })
        } else {
          resolve(new Response('url error:' + event.request.url));
        }
      }));
  });
