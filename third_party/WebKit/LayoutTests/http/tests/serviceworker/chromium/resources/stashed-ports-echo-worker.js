
self.onmessage = function(e) {
  var port = self.ports.add(e.data.name, e.data.port);
};

self.ports.onmessage = function(e) {
  e.source.postMessage({name: e.source.name, data: e.data});
};
