self.addEventListener('fetch', event => {
    if (!event.request.url.match(/opaque-response$/))
      return;
    event.respondWith(fetch("http://localhost:8000/serviceworker/resources/simple.txt", {mode: 'no-cors'}));
  });
