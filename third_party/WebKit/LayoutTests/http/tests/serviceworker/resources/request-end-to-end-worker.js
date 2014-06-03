var port = undefined;

onmessage = function(e) {
    var message = e.data;
    if (typeof message === 'object' && 'port' in message) {
        port = message.port;
    }
};

onfetch = function(e) {
    var headers = {};
    e.request.headers.forEach(function(value, key) {
        headers[key] = value;
    });
    port.postMessage({
        url: e.request.url,
        method: e.request.method,
        headers: headers,
        headerSize: e.request.headers.size
    });
}