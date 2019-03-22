self.addEventListener('fetch', fetchEvent => {
  console.log('service worker making fetch for url: ' + fetchEvent.request.url);
  fetch(fetchEvent.request);
});
