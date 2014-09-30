var port = undefined;

onmessage = function(e) {
    var message = e.data;
    if (typeof message === 'object' && 'port' in message) {
        port = message.port;
    }
};

onfetch = function(e) {
    var headers = {};
    for (var header of e.request.headers) {
      var key = header[0], value = header[1];
      headers[key] = value;
    }
    port.postMessage({
        url: e.request.url,
        method: e.request.method,
        headers: headers,
        headerSize: e.request.headers.size
    });
}
