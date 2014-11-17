// Test that multiple fetch handlers do not confuse the implementation.
self.addEventListener('fetch', function(event) {});

self.addEventListener('fetch', function(event) {
    var testcase = new URL(event.request.url).search;
    switch (testcase) {
    case '?reject':
      event.respondWith(Promise.reject());
      break;
    case '?prevent-default':
      event.preventDefault();
      break;
    case '?prevent-default-and-respond-with':
      event.preventDefault();
      break;
    }
  });

self.addEventListener('fetch', function(event) {});

self.addEventListener('fetch', function(event) {
    var testcase = new URL(event.request.url).search;
    if (testcase == '?prevent-default-and-respond-with')
      event.respondWith(new Response('responding!'));
  });

self.addEventListener('fetch', function(event) {});
