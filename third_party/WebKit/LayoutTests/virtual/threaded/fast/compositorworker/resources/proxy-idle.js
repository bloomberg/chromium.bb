self.onmessage = function(msg) {
  var proxy = msg.data;
  postMessage('started');
}
