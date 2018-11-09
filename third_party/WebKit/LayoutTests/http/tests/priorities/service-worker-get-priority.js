addEventListener('fetch', e => {
  if (e.request.url.endsWith('getPriority'))
    e.respondWith(fetchAndMessagePriority(e.request));
});

async function fetchAndMessagePriority(request) {
  const response = await fetch(request);
  const priority = internals.getResourcePriority(request.url, this);
  const clientArray = await clients.matchAll({includeUncontrolled: true});
  clientArray.forEach(client => {
    client.postMessage(priority);
  });

  return response;
}
