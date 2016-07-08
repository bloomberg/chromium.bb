self.onmessage = function(msg) {
  awaitProxyInit(msg.data.proxy).then((proxy) => {
    proxy.opacity = 0.5;
    var transform = proxy.transform;
    transform.m42 = 100.0;
    proxy.transform = transform;
    proxy.scrollLeft = 100;
    proxy.scrollTop = 100;
    postMessage({});
  });
}

function awaitProxyInit(proxy) {
  return new Promise((resolve, reject) => {
    function check() {
      if (proxy.initialized)
        resolve(proxy);
      else
        requestAnimationFrame(check);
    }
    requestAnimationFrame(check);
  });
}
