self.onmessage = function(msg) {
  console.log(msg.data);
  requestAnimationFrame(function() {
    postMessage(msg.data);
  });
}