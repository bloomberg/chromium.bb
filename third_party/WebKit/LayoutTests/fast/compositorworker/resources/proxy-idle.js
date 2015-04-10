var proxy;
onmessage = function(event) {
    proxy = event.data;
    postMessage('started');
}
