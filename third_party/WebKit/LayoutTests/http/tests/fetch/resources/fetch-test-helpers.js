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

// Method names.

// A method name must match token in RFC 2616:
// "token          = 1*<any CHAR except CTLs or separators>"
// Fetch API Spec: https://fetch.spec.whatwg.org/#concept-method
// FIXME: Add ''. https://crbug.com/455162
// All octets are tested except for those >= 0x80.
var INVALID_METHOD_NAMES = [
  // CTL
  '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
  '\x08', '\x09', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
  '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
  '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f', '\x7f',
  // separators
  ' ', '"', '(', ')', ',', '/', ':', ';', '<', '=', '>', '?', '@', '[', '\\',
  ']', '{', '}',
  // non-CHAR
  '\x80', '\xff', '\u0100', '\u3042',
  // Strings that contain characters above.
  'a(b', 'invalid name', 'invalid \r name', 'invalid \n name',
  'invalid\r\n name', 'invalid \0 name',
  'test\r', 'test\n', 'test\r\n', 'test\0',
  '\0'.repeat(100000), '<'.repeat(100000), '\r\n'.repeat(50000),
  'x'.repeat(100000) + '\0'];

// Spec: https://fetch.spec.whatwg.org/#forbidden-method
// "A forbidden method is a method that is a byte case-insensitive match for
// one of `CONNECT`, `TRACE`, and `TRACK`."
var FORBIDDEN_METHODS = ['TRACE', 'TRACK', 'CONNECT',
                         'trace', 'track', 'connect'];

// Spec: https://fetch.spec.whatwg.org/#concept-method-normalize
// "To normalize a method, if it is a byte case-insensitive match for
// `DELETE`, `GET`, `HEAD`, `OPTIONS`, `POST`, or `PUT`, byte uppercase it"
var TO_BE_NORMALIZED_METHOD_NAMES = [
  'delete', 'get', 'head', 'options', 'post', 'put'];

var OTHER_VALID_METHOD_NAMES = [
  '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '^', '_', '`', '|', '~',
  '0123456789', 'PATCH', 'MKCOL', 'CUSTOM', 'X-FILES', 'p0sT', 'AZaz',
  'x'.repeat(100000)];

// token [RFC 2616]
var INVALID_TOKENS = INVALID_METHOD_NAMES.concat(['']);
var VALID_TOKENS = FORBIDDEN_METHODS
  .concat(TO_BE_NORMALIZED_METHOD_NAMES)
  .concat(OTHER_VALID_METHOD_NAMES);

// Header names and values.

// A header name must match token in RFC 2616.
// Fetch API Spec: https://fetch.spec.whatwg.org/#concept-header-name
var INVALID_HEADER_NAMES = INVALID_TOKENS;
var INVALID_HEADER_VALUES = [
  'test \r data', 'test \n data', 'test \0 data',
  'test\r\n data',
  'test\r', 'test\n', 'test\r\n', 'test\0',
  '\0'.repeat(100000), '\r\n'.repeat(50000), 'x'.repeat(100000) + '\0'];

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
