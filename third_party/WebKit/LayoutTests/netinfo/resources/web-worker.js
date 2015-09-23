addEventListener('message', function(e) {
    self.postMessage(navigator.connection.type + ',' + navigator.connection.downlinkMax);
}, false);

navigator.connection.addEventListener('change', function() {
    self.postMessage(navigator.connection.type + ',' + navigator.connection.downlinkMax);
}, false);