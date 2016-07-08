self.onmessage = function(msg) {
  if (msg.data.proxy)
    self.proxy = msg.data.proxy;

  awaitProxyInit(self.proxy).then((proxy) => {
    var keys = ['opacity', 'scrollTop', 'scrollLeft'];
    var values = {};
    for (var i = 0; i < keys.length; i++) {
      values[keys[i]] = proxy[keys[i]];
    }
    values.transform = proxy.transform.toFloat32Array();
    postMessage(values);
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
