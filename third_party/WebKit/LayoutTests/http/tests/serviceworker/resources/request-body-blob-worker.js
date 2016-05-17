self.addEventListener('fetch', event => {
    if (event.request.url.indexOf('?post-with-blob-body') == -1)
      return;
    event.respondWith(
     event.request.text().then(resp => {
       if (resp.indexOf('it\'s me the blob') == -1)
         return new Response('fail');
       if (resp.indexOf('and more blob!') == -1)
         return new Response('fail');
       return new Response('Pass');
     }));
  });
