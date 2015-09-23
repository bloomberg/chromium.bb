onmessage = function(event) {
    var proxy = event.data;
    proxy.opacity;
    proxy.disconnect();
    try {
        proxy.opacity;
    } catch (e) {
        postMessage(e.name);
    }
}
