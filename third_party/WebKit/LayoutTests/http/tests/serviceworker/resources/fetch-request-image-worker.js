var requests = [];

self.onmessage = function(e) {
  var message = e.data;
  if ('port' in message) {
    var port = message.port;
    port.postMessage(requests);
  }
};

self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    if (url.indexOf('dummy?test') == -1) {
      return;
    }
    requests[url] = {
      mode: event.request.mode,
    };
    event.respondWith(Promise.reject());
  });
