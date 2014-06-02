addEventListener('message', function(e) {
    self.postMessage(navigator.connection.type);
}, false);

navigator.connection.addEventListener('typechange', function() {
    self.postMessage(navigator.connection.type);
}, false);
