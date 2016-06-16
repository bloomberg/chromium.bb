function awaitProxyInit(proxy) {
  return new Promise((resolve, reject) => {
    function check() {
      if (proxy.initialized)
        resolve(proxy);
      requestAnimationFrame(check);
    }
    requestAnimationFrame(check);
  });
}
