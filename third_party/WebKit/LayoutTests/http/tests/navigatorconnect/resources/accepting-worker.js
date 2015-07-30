navigator.services.addEventListener('connect', function(event) {
  event.respondWith({accept: true});
});
