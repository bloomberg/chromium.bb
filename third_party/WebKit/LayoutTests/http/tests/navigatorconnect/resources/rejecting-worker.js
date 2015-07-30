navigator.services.addEventListener('connect', function(event) {
  event.respondWith({accept: false});
});
