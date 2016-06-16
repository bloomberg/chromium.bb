self.importScripts('worker-common.js');

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
