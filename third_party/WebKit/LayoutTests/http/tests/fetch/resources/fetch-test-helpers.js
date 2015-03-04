if ('ServiceWorkerGlobalScope' in self &&
    self instanceof ServiceWorkerGlobalScope) {
  // ServiceWorker case
  importScripts('/serviceworker/resources/worker-testharness.js');
  importScripts('/serviceworker/resources/test-helpers.js');
  importScripts('/serviceworker/resources/fetch-test-options.js');
} else if (self.importScripts) {
  // Other workers cases
  importScripts('/resources/testharness.js');
  importScripts('/serviceworker/resources/test-helpers.js');
  importScripts('/serviceworker/resources/fetch-test-options.js');
}

function getContentType(headers) {
  var content_type = '';
  for (var header of headers) {
    if (header[0] == 'content-type')
      content_type = header[1];
  }
  return content_type;
}


(function () {
  var sequential_promise = Promise.resolve();

  // |sequential_test| is a dummy test to prevent tests from terminating early.
  // Without this, the number of pending tests recognized by testharness can
  // be zero, which leads to early unexpected test termination.
  var sequential_test = undefined;

  // sequential_promise_test() is similar to promise_test(), but multiple tests
  // are executed one by one sequentially.
  // Also call sequential_promise_test_done() after all
  // sequential_promise_test() are called.
  function sequential_promise_test(func, name) {
    if (sequential_test === undefined) {
      sequential_test = async_test('Sequential');
    }
    sequential_promise = sequential_promise
      .then(function() {
          var promise = undefined;
          promise_test(function(t) {
              promise = func(t);
              return promise;
            }, name);
          return promise;
        })
      .catch(function(e) {
          // The error was already reported. Continue to the next test.
        });
  }

  function sequential_promise_test_done() {
    if (sequential_test !== undefined) {
      sequential_promise.then(function() {sequential_test.done();});
    }
  }

  self.sequential_promise_test = sequential_promise_test;
  self.sequential_promise_test_done = sequential_promise_test_done;
})();

// token [RFC 2616]
// "token          = 1*<any CHAR except CTLs or separators>"
// All octets are tested except for those >= 0x80.
var INVALID_TOKENS = [
  '',
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

// Method names.

// A method name must match token in RFC 2616:
// Fetch API Spec: https://fetch.spec.whatwg.org/#concept-method
var INVALID_METHOD_NAMES = INVALID_TOKENS;

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

// ResponseInit's statusText must match Reason-Phrase.
// https://fetch.spec.whatwg.org/#dom-response Step 2.
var INVALID_REASON_PHRASE = [
  // \x00-\x1F (except for \t) and \x7f are invalid.
  '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
  '\x08', '\x0a', '\x0b', '\x0c', '\x0d', '\x0e', '\x0f',
  '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
  '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f', '\x7f',
  // non-ByteString.
  '\u0100', '\u3042',
  // Strings that contain characters above.
  'invalid \r reason-phrase', 'invalid \n reason-phrase',
  'invalid \0 reason-phrase', 'invalid\r\n reason-phrase',
  'test\r', 'test\n', 'test\r\n', 'test\0',
  '\0'.repeat(100000), '\r\n'.repeat(50000),
  'x'.repeat(100000) + '\0'];

var VALID_REASON_PHRASE = [
  '\t', ' ', '"', '(', ')', ',', '/', ':', ';', '<', '=', '>', '?', '@', '[',
  '\\', ']', '{', '}',
  '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '^', '_', '`', '|', '~',
  // non-CHAR
  '\x80', '\xff',
  // Valid strings.
  '', '0123456789', '404 Not Found', 'HTTP/1.1 404 Not Found', 'AZ\u00ffaz',
  'x'.repeat(100000)];

function testBlockMixedContent(mode) {
  promise_test(function(t) {
      return Promise.resolve()
        .then(function() {
            // Test 1: Must fail: blocked as mixed content.
            return fetch(BASE_URL + 'test1-' + mode, {mode: mode})
              .then(t.unreached_func('Test 1: Must be blocked (' +
                                     mode + ', HTTPS->HTTP)'),
                    function() {});
          })
        .then(function() {
            // Block mixed content in redirects.
            // Test 2: Must fail: original fetch is not blocked but
            // redirect is blocked.
            return fetch(HTTPS_REDIRECT_URL +
                         encodeURIComponent(BASE_URL + 'test2-' + mode),
                         {mode: mode})
              .then(t.unreached_func('Test 2: Must be blocked (' +
                                     mode + ', HTTPS->HTTPS->HTTP)'),
                    function() {});
          })
        .then(function() {
            // Test 3: Must fail: original fetch is blocked.
            return fetch(REDIRECT_URL +
                         encodeURIComponent(HTTPS_BASE_URL + 'test3-' + mode),
                         {mode: mode})
              .then(t.unreached_func('Test 3: Must be blocked (' +
                                     mode + ', HTTPS->HTTP->HTTPS)'),
                    function() {});
          })
        .then(function() {
            // Test 4: Must success.
            // Test that the mixed contents above are not rejected due to
            return fetch(HTTPS_REDIRECT_URL +
                         encodeURIComponent(HTTPS_BASE_URL + 'test4-' + mode),
                         {mode: mode})
              .then(function(res) {assert_equals(res.status, 200); },
                    t.unreached_func('Test 4: Must success (' +
                                     mode + ', HTTPS->HTTPS->HTTPS)'));
          })
        .then(function() {
            // Test 5: Must success if mode is not 'same-origin'.
            // Test that the mixed contents above are not rejected due to
            // CORS check.
            return fetch(HTTPS_OTHER_REDIRECT_URL +
                         encodeURIComponent(HTTPS_BASE_URL + 'test5-' + mode),
                         {mode: mode})
              .then(function(res) {
                  if (mode === 'same-origin') {
                    assert_unreached(
                      'Test 5: Cross-origin HTTPS request must fail: ' +
                      'mode = ' + mode);
                  }
                },
                function() {
                  if (mode !== 'same-origin') {
                    assert_unreached(
                      'Test 5: Cross-origin HTTPS request must success: ' +
                      'mode = ' + mode);
                  }
                });
          });
    }, 'Block fetch() as mixed content (' + mode + ')');
}
