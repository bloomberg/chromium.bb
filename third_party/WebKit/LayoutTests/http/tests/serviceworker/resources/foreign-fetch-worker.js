self.addEventListener('install', function(event) {
    event.registerForeignFetchScopes([registration.scope + '/intercept']);
  });

self.addEventListener('fetch', function(event) {
    event.respondWith(new Response('Foreign Fetch'));
  });
