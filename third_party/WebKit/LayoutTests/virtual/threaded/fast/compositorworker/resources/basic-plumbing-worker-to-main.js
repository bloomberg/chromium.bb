self.onmessage = function(msg) {
  if (msg.data.proxy)
    self.proxy = msg.data.proxy;

  awaitProxyInit(self.proxy).then((proxy) => {
    proxy.opacity = 0.5;
    var transform = proxy.transform;
    transform.m41 = 30.0;
    transform.m44 = 2;
    proxy.transform = transform;
    proxy.scrollLeft = 10;
    proxy.scrollTop = 20;
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
