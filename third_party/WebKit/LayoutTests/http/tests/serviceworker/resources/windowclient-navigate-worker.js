var port = null;

self.onmessage = function(e) {
  port = e.data.port;
  port.onmessage = on_message;
  port.postMessage('ready');
};

function on_message(e) {
  var url = e.data;
  var client;

  self.clients.matchAll({ includeUncontrolled : true }).then(function(cs) {
        cs.forEach(function(c) { c.frameType == 'nested' && (client = c); });
        return client.navigate(url);
      })
    .then(function(c) {
        if (!c)
          port.postMessage(c);
        else
          port.postMessage(c.url);
      })
    .catch(function(e) {
        port.postMessage(e.name);
      });
}
