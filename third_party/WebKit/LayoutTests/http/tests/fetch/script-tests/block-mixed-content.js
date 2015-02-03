if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

var BASE_URL =
  'http://127.0.0.1:8000/serviceworker/resources/fetch-access-control.php?ACAOrigin=*&label=';
var HTTPS_BASE_URL =
  'https://127.0.0.1:8443/serviceworker/resources/fetch-access-control.php?ACAOrigin=*&label=';
var HTTPS_OTHER_BASE_URL =
  'https://localhost:8443/serviceworker/resources/fetch-access-control.php?ACAOrigin=*&label=';

var REDIRECT_URL =
  'http://127.0.0.1:8000/serviceworker/resources/redirect.php?ACAOrigin=*&Redirect=';
var HTTPS_REDIRECT_URL =
  'https://127.0.0.1:8443/serviceworker/resources/redirect.php?ACAOrigin=*&Redirect=';
var HTTPS_OTHER_REDIRECT_URL =
  'https://localhost:8443/serviceworker/resources/redirect.php?ACAOrigin=*&Redirect=';

['same-origin', 'cors', 'no-cors'].forEach(function(mode) {
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
  });

done();
