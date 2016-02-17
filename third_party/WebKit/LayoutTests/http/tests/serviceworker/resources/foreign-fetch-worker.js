self.addEventListener('install', function(event) {
    var origins = JSON.parse(decodeURIComponent(location.search.substring(1)));
    event.registerForeignFetchScopes([registration.scope + '/intercept'],
                                     origins);
  });

self.addEventListener('foreignfetch', function(event) {
    event.respondWith(new Response('Foreign Fetch'));
  });
