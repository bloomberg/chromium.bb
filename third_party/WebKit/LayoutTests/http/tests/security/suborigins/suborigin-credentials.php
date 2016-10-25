<?php
header("Suborigin: foobar");
setcookie("TestCookieSuboriginCredentialsSamePO", "cookieisset", 0, "/");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Credentials are not sent by default from suborigin to same physical origin</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<div id="container"></div>
<script>
var same_physical_origin_url = '/security/resources/cors-script.php?' +
  'resultvar=result1&cookie=TestCookieSuboriginCredentialsSamePO&' +
  'credentials=true&' +
  'cors=http-so://foobar.127.0.0.1:8000';
var diff_physical_origin_url =
  'http://localhost:8000/security/resources/cors-script.php?' +
  'resultvar=result2&cookie=TestCookieSuboriginCredentialsDiffPO&' +
  'credentials=true&' +
  'cors=http-so://foobar.127.0.0.1:8000';

var redirect_to_same_physical_origin_url_params = new URLSearchParams();
redirect_to_same_physical_origin_url_params.append(
  'url', same_physical_origin_url);
var same_physical_origin_with_redirect_url =
  '/security/suborigins/resources/redirect.php?' +
  'credentials=true&' +
  'cors=http-so://foobar.127.0.0.1:8000&' +
  redirect_to_same_physical_origin_url_params.toString();

var cross_origin_iframe_loaded_resolver = undefined;
var cross_origin_iframe_loaded = new Promise(function(resolve) {
    cross_origin_iframe_loaded_resolver = resolve;
});

// Waits for a message from the iframe so it knows the cross-origin iframe
// cookie is set.
window.addEventListener('message', function() {
    cross_origin_iframe_loaded_resolver();
  });

var iframe = document.createElement('iframe');
iframe.src =
  'http://localhost:8000/security/suborigins/resources/' +
  'set-cookie-and-postmessage-parent.php?' +
  'name=TestCookieSuboriginCredentialsDiffPO&' +
  'value=cookieisset';
document.body.appendChild(iframe);

function xhr_test(url, result_name, expected_value) {
  return function(test) {
      var xhr = new XMLHttpRequest();
      xhr.onload = test.step_func(_ => {
          assert_equals(
            xhr.responseText, result_name + ' = "' + expected_value + '";');
          test.done();
        });
      xhr.onerror = test.unreached_func();

      xhr.open('GET', url);
      xhr.send();
    };
}

async_test(
  xhr_test(same_physical_origin_url, 'result1', ''),
  'No credentials by default with XHR to same physical origin');

async_test(
  xhr_test(same_physical_origin_with_redirect_url, 'result1', ''),
  'No credentials by default across redirect with XHR to same physical origin');

async_test(
  xhr_test(diff_physical_origin_url, 'result2', ''),
  'No credentials by default with XHR to different physical origin');

function fetch_test(url, result_name, expected_value) {
  return function(test) {
    return fetch(url, { credentials: 'same-origin' })
      .then(r => {
          return r.text()
            .then(test.step_func(text => {
                  assert_equals(
                    text, result_name + ' = "' + expected_value + '";');
              }))
        });
    };
}

promise_test(
  fetch_test(same_physical_origin_url, 'result1', ''),
  'No credentials with same-origin Fetch to same physical origin');

promise_test(
  fetch_test(same_physical_origin_with_redirect_url, 'result1', ''),
  'No credentials across redirect with same-origin Fetch to same ' +
  'physical origin');

promise_test(
  fetch_test(diff_physical_origin_url, 'result2', ''),
  'No credentials with same-origin Fetch to different physical origin');

function script_element_test(url, result_name, expected_value) {
  return function(test) {
      window[result_name] = false;
      var script = document.createElement('script');
      script.setAttribute('crossOrigin', 'anonymous');
      script.src = url;
      script.onload = test.step_func(function() {
          assert_equals(window[result_name], expected_value);
          test.done();
        });
      document.body.appendChild(script);
    };
}

async_test(
  script_element_test(same_physical_origin_url, 'result1', ''),
  'No credentials with crossorigin="anonymous" script tag to same ' +
  'physical origin');

async_test(
  script_element_test(same_physical_origin_with_redirect_url,
    'result1', ''),
  'No credentials across redirect with crossorigin="anonymous" script tag ' +
  'to same physical origin');

async_test(
  script_element_test(diff_physical_origin_url, 'result2', ''),
  'No credentials with crossorigin="anonymous" script tag to different ' +
  'physical origin');
</script>
</body>
</html>
