// This service worker, once installed, will reject all resource requests,
// unless the resource has 'bypass' in the URL. For those URLs, it simply
// passes through the request.
self.addEventListener('fetch', function(event) {
    if (event.request.url.indexOf('bypass') != -1)
        event.respondWith(fetch.event.request);
    event.respondWith(Promise.reject(Error('Rejecting all requests')));
});
