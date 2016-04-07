self.addEventListener('install', function(event) {
    var origins = JSON.parse(decodeURIComponent(location.search.substring(1)));
    event.registerForeignFetch({scopes: [registration.scope + '/intercept'],
                                origins: origins});
  });

self.addEventListener('foreignfetch', function(event) {
    event.respondWith({response: new Response('Foreign Fetch')});
  });
