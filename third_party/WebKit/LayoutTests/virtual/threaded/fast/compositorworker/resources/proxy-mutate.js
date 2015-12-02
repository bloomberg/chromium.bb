onmessage = function(msg) {
    var proxy = msg.data[0];
    var attrib = msg.data[1];
    try {
        if (proxy.supports(attrib)) {
            proxy[attrib] = 0;
            postMessage('success');
        } else {
            postMessage('error: Received non-supported attribute "' + attrib + '"');
        }
    } catch (e) {
        postMessage('error: ' + e);
    }
}
