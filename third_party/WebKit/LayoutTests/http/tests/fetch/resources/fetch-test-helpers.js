if ('ServiceWorkerGlobalScope' in self &&
    self instanceof ServiceWorkerGlobalScope) {
  // ServiceWorker case
  importScripts('/serviceworker/resources/worker-testharness.js');
  importScripts('/resources/testharness-helpers.js');
  importScripts('/serviceworker/resources/test-helpers.js');
} else if (self.importScripts) {
  // Other workers cases
  importScripts('/resources/testharness.js');
  importScripts('/resources/testharness-helpers.js');
  importScripts('/serviceworker/resources/test-helpers.js');
}

// FIXME: unreached_rejection is duplicated so should be removed.
// Rejection-specific helper that provides more details
function unreached_rejection(test, prefix) {
  return test.step_func(function(error) {
      var reason = error.message || error.name || error;
      var error_prefix = prefix || 'unexpected rejection';
      assert_unreached(error_prefix + ': ' + reason);
    });
}

function getContentType(headers) {
  var content_type = '';
  for (var header of headers) {
    if (header[0] == 'content-type')
      content_type = header[1];
  }
  return content_type;
}

var FORBIDDEN_HEADERS =
  ['Accept-Charset', 'Accept-Encoding', 'Access-Control-Request-Headers',
   'Access-Control-Request-Method', 'Connection', 'Content-Length',
   'Cookie', 'Cookie2', 'Date', 'DNT', 'Expect', 'Host', 'Keep-Alive',
   'Origin', 'Referer', 'TE', 'Trailer', 'Transfer-Encoding', 'Upgrade',
   'User-Agent', 'Via', 'Proxy-', 'Sec-', 'Proxy-FooBar', 'Sec-FooBar'];
var FORBIDDEN_RESPONSE_HEADERS = ['Set-Cookie', 'Set-Cookie2'];
var SIMPLE_HEADERS =
  [['Accept', '*'], ['Accept-Language', 'ru'], ['Content-Language', 'ru'],
   ['Content-Type', 'application/x-www-form-urlencoded'],
   ['Content-Type', 'multipart/form-data'],
   // MIME types are case-insensitive.
   ['Content-Type', 'multiPart/foRm-data'],
   // MIME-type parameters are ignored when determining simple headers.
   ['Content-Type', 'multiPart/foRm-data;charset=utf-8'],
   ['Content-Type', 'text/plain']];
var NON_SIMPLE_HEADERS =
  [['X-Fetch-Test', 'test'],
   ['X-Fetch-Test2', 'test2'],
   ['Content-Type', 'foo/bar']];
