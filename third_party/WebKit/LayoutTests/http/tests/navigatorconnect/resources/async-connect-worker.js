navigator.services.addEventListener('connect', function(event) {
  var targetUrl = new URL(event.targetURL);
  event.respondWith(new Promise(function(resolve, reject) {
      if (targetUrl.search == "?accept")
        self.setTimeout(resolve, 1, {accept: true});
      else
        self.setTimeout(reject, 1);
    }));
});
