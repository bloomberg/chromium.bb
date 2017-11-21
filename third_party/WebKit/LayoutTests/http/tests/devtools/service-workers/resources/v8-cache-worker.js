const cacheName = 'c8-cache-test';
const scriptName = 'v8-cache-script.js';

let continue_install;
let install_finished;
const install_finished_promise = new Promise((r) => { install_finished = r; });

self.addEventListener('install', (event) => {
    event.waitUntil(
      new Promise((resolve) => { continue_install = resolve; })
        .then(() => caches.open(cacheName))
        .then((cache) => cache.add(new Request(scriptName)))
        .then(() => { install_finished(); }));
  });

self.addEventListener('message', (event) => {
    continue_install();
    install_finished_promise.then(() => {
      event.data.port.postMessage('');
    });
  });

self.addEventListener('fetch', (event) => {
    if (event.request.url.indexOf(scriptName) == -1) {
      return;
    }
    event.respondWith(
      caches.open(cacheName)
        .then((cache) => {
          return cache.match(new Request(scriptName));
        })
    );
  });
