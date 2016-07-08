self.onmessage = function(msg) {
  var proxy = msg.data[0];
  var attrib = msg.data[1];

  awaitProxyInit(proxy).then((proxy) => {
    try {
      if (proxy.supports(attrib)) {
        proxy[attrib] = 0;
        postMessage('success');
      } else {
        postMessage('error: Received non-supported attribute "' + attrib + '"');
      }
    } catch (e) {
      postMessage('error: ' + e);
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
