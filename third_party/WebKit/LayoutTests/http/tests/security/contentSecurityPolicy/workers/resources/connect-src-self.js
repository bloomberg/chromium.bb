importScripts("http://127.0.0.1:8000/resources/testharness.js");
importScripts("http://127.0.0.1:8000/security/contentSecurityPolicy/resources/testharness-helper.js");

// Same-origin
async_test(t => {
  var url = "http://127.0.0.1:8000/security/resources/cors-hello.php?same-origin-fetch";
  assert_no_csp_event_for_url(t, url);

  fetch(url)
    .then(t.step_func_done(r => assert_equals(r.status, 200)));
}, "Same-origin 'fetch()' in " + self.location.protocol);

async_test(t => {
  var url = "http://127.0.0.1:8000/security/resources/cors-hello.php?same-origin-xhr";
  assert_no_csp_event_for_url(t, url);

  var xhr = new XMLHttpRequest();
  try {
    xhr.open("GET", url);
    t.done();
  } catch (e) {
    assert_unreached();
  }
  xhr.send();
}, "Same-origin XHR in " + self.location.protocol);

// Cross-origin
async_test(t => {
  var url = "http://example.test:8000/security/resources/cors-hello.php?cross-origin-fetch";

  Promise.all([
    waitUntilCSPEventForURL(t, url),
    fetch(url)
      .catch(t.step_func(e => assert_true(e instanceof TypeError)))
  ]).then(_ => t.done());
}, "Cross-origin 'fetch()' in " + self.location.protocol);

async_test(t => {
  var url = "http://example.test:8000/security/resources/cors-hello.php?cross-origin-xhr";

  Promise.all([
    waitUntilCSPEventForURL(t, url),
    new Promise((resolve, reject) => {
      var xhr = new XMLHttpRequest();
      try {
        xhr.open("GET", url);
        reject("xhr.open should have thrown");
      } catch (e) {
        resolve();
      }
    })
  ]).then(_ => t.done());
}, "Cross-origin XHR in " + self.location.protocol);

// Same-origin redirecting to cross-origin
async_test(t => {
  var url = "http://127.0.0.1:8000/security/resources/redir.php?url=http://example.test:8000/security/resources/cors-hello.php?cross-origin-fetch";
  // TODO(mkwst): The event should be firing. :(

  fetch(url)
    .catch(t.step_func_done(e => assert_true(e instanceof TypeError)))
}, "Same-origin => cross-origin 'fetch()' in " + self.location.protocol);

done();
