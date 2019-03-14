async function postMessageToWindow(msg) {
  const matchedClients = await clients.matchAll({includeUncontrolled: true});
  for (const client of matchedClients)
    client.postMessage(msg);
}

self.addEventListener(
    'backgroundfetchsuccess',
    event => postMessageToWindow(`Completed "${event.registration.id}"`));
