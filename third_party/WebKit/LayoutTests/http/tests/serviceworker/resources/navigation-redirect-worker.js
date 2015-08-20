// TODO(horo): Service worker can be killed at some point during the test. So we
// should use storage API instead of this global variable.
var urls = [];

self.addEventListener('message', function(event) {
    event.data.port.postMessage({urls: urls});
    urls = [];
  });

function get_query_params(url) {
  var search = (new URL(url)).search;
  if (!search) {
    return {};
  }
  var ret = {};
  var params = search.substring(1).split('&');
  params.forEach(function(param) {
      var element = param.split('=');
      ret[decodeURIComponent(element[0])] = decodeURIComponent(element[1]);
    });
  return ret;
}

self.addEventListener('fetch', function(event) {
    urls.push(event.request.url)
    var params = get_query_params(event.request.url);
    if (params['sw'] == 'gen') {
      event.respondWith(Response.redirect(params['url']));
    } else if (params['sw'] == 'fetch') {
      event.respondWith(fetch(event.request));
    } else if (params['sw'] == 'opaque') {
      event.respondWith(fetch(
          new Request(event.request.url, {redirect: 'manual'})));
    } else if (params['sw'] == 'opaqueThroughCache') {
      var url = event.request.url;
      var cache;
      event.respondWith(
          self.caches.delete(url)
            .then(function() { return self.caches.open(url); })
            .then(function(c) {
                cache = c;
                return fetch(new Request(url, {redirect: 'manual'}));
              })
            .then(function(res) { return cache.put(event.request, res); })
            .then(function() { return cache.match(url); }));
    }
  });
