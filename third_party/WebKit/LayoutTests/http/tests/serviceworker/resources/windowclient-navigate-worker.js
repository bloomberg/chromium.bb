function match_query(query_string) {
  return self.location.search.substr(1) == query_string;
}

function receive_event(event_name) {
  return new Promise(function(resolve) {
        self.addEventListener(event_name, resolve, false);
      });
}

function navigate_test(e) {
  var port = e.data.port;
  var url = e.data.url;

  return clients.matchAll({ includeUncontrolled : true })
    .then(function(client_list) {
        for (var i = 0; i < client_list.length; i++) {
          var client = client_list[i];
          if (client.frameType == 'nested') {
            return client.navigate(url);
          }
        }
        port.postMessage('Could not found window client.');
      })
    .then(function(new_client) {
        if (new_client === null)
          port.postMessage(new_client);
        else
          port.postMessage(new_client.url);
      })
    .catch(function(error) {
        port.postMessage(error.name);
      });
}
if (match_query('installing')) {
  // If the query string is "?installing", then do test on installing worker.
  // This is only for in-scope-but-not-controlled test.
  receive_event('install').then(function(e) {
      e.waitUntil(receive_event('message').then(navigate_test));
    });
} else {
  receive_event('message').then(function(e) {
      e.waitUntil(navigate_test(e));
    });
}
