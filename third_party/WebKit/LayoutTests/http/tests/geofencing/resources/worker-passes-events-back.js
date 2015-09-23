var port;

self.addEventListener('message', function(event) {
  port = event.data.port;
  port.postMessage('setup');
});

self.addEventListener('geofenceenter', function(event) {
  port.postMessage({event: 'geofenceenter', id: event.region.id});
});

self.addEventListener('geofenceleave', function(event) {
  port.postMessage({event: 'geofenceleave', id: event.region.id});
});
