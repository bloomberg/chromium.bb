self.importScripts('worker-common.js');

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
