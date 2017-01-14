<?php
if (isset($_GET["origintrial"])) {
  // generate_token.py  http://127.0.0.1:8000 ServiceWorkerNavigationPreload -expire-timestamp=2000000000
  header("Origin-Trial: AsAA4dg2Rm+GSgnpyxxnpVk1Bk8CcE+qVBTDpPbIFNscyNRJOdqw1l0vkC4dtsGm1tmP4ZDAKwycQDzsc9xr7gMAAABmeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiU2VydmljZVdvcmtlck5hdmlnYXRpb25QcmVsb2FkIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
}
header('Content-Type: application/javascript');
?>
var log = '';

self.addEventListener('activate', event => {
  log += 'Activate event handler\n';
  if (!self.registration.navigationPreload) {
    log += 'registration.navigationPreload not found\n';
    return;
  }
  event.waitUntil(
    registration.navigationPreload.disable()
      .then(
        result => { log += 'disable() resolved with: ' + result + '\n';},
        error => { log += 'disable() rejected with: ' + error + '\n';})
      .then(_ => registration.navigationPreload.enable())
      .then(
        result => { log += 'enable() resolved with: ' + result + '\n';},
        error => { log += 'enable() rejected with: ' + error + '\n';})
      .then(_ => registration.navigationPreload.getState())
      .then(
        result => {
          log += 'getState() resolved with: ' + JSON.stringify(result) + '\n';
        },
        error => { log += 'getState() rejected with: ' + error + '\n';})
      .then(_ => registration.navigationPreload.setHeaderValue('world'))
      .then(
        result => { log += 'setHeaderValue() resolved with: ' + result + '\n';},
        error => { log += 'setHeaderValue() rejected with: ' + error + '\n';}));
});

self.addEventListener('message', event => {
  event.source.postMessage(log);
});

self.addEventListener('fetch', event => {
  log += 'Fetch event handler\n';
  if (!event.preloadResponse) {
    log += 'event.preloadResponse not found\n';
  } else {
    event.respondWith(
      event.preloadResponse
      .then(
          res => {
            log += 'event.preloadResponse resolved with: ' + res + '\n';
            if (!res) {
              return new Response('');
            }
            return res.text().then(text => {
              log += 'Text of preloadResponse: [' + text + ']\n';
              return new Response('');
            })
          },
          err => {
            log += 'event.preloadResponse rejected with: ' + err + '\n';
            return new Response('');
          }));
  }
});
