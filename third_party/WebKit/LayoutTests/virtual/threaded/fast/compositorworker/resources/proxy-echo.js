self.onmessage = function(msg) {
  var proxy = msg.data;
  postMessage({
    opacity: proxy.supports("opacity"),
    transform: proxy.supports("transform"),
    proxy: proxy
  });
}
