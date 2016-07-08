self.onmessage = function(msg) {
  awaitProxyInit(msg.data).then((proxy) => {
    proxy.opacity;
    proxy.disconnect();
    try {
      proxy.opacity;
      postMessage("Able to read mutable property after disconnect.");
    } catch (e) {
      postMessage(e.name);
    }
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
