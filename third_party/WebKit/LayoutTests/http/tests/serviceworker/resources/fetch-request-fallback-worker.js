// TODO(horo): Service worker can be killed at some point during the test. So we
// should use storage API instead of this global variable.
var requests = [];

self.addEventListener('message', function(event) {
    event.data.port.postMessage({requests: requests});
    requests = [];
  });

self.addEventListener('fetch', function(event) {
    requests.push({
        url: event.request.url,
        mode: event.request.mode
      });
  });
