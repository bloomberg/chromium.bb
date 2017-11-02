let source;

self.addEventListener('message', event => {
    source = event.source;
  });

self.addEventListener('fetch', event => {
    if (event.request.url.indexOf('fetch-with-body') != -1) {
      event.waitUntil(Promise.resolve()
        .then(() => { return event.request.text(); })
        .then(body => { source.postMessage(body); }));
    }
  });
