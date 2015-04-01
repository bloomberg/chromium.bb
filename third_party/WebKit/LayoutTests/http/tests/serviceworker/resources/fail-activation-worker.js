onactivate = function(e) {
  e.waitUntil(clients.claim().then(function() { return Promise.reject(); }));
};
