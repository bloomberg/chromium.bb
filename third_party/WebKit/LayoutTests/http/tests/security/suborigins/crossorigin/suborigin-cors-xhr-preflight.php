<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Allow suborigin in HTTP header</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script src="/security/suborigins/resources/suborigin-cors-lib.js"></script>
</head>
<body>
<div id="container"></div>
<script>
// XMLHttpRequest tests
var SuboriginXHRTest = function(pass, name, src, crossorigin_value) {
  SuboriginTest.call(this, pass, 'XHR: ' + name, src, crossorigin_value);
}

SuboriginXHRTest.prototype.execute = function() {
  var settings = this;
  async_test(test => {
      var xhr = new XMLHttpRequest();

      if (settings.crossorigin_value === 'use-credentials') {
        xhr.withCredentials = true;
      }

      if (settings.pass) {
        xhr.onload = test.step_func_done();
        xhr.onerror = test.unreached_func('Good XHR fired error handler.');
      } else {
        xhr.onload = test.unreached_func('Bad XHR successful.');
        xhr.onerror = test.step_func_done();
      }

      xhr.open('GET', settings.src);
      // Set a custom header to force a preflight. Even though the
      // scheme/host/port of the source and destination origins are the same, the
      // Suborigin should cause the request to be treated as cross-origin.
      xhr.setRequestHeader('x-custom-header', 'foobar');
      xhr.send();
    }, settings.name);
};

var xorigin_preflight_script =
  'http://127.0.0.1:8000/security/resources/cors-script.php';

// XHR preflight tests
new SuboriginXHRTest(
  false,
  'Complex anonymous XHR preflight, no AC for custom header',
  xorigin_preflight_script + '?cors=http-so://foobar.127.0.0.1:8000',
  'anonymous').execute();

new SuboriginXHRTest(
  true,
  'Complex anonymous XHR preflight, has AC for custom header',
  xorigin_preflight_script + '?cors=http-so://foobar.127.0.0.1:8000&' +
  'custom=x-custom-header',
  'anonymous').execute();

new SuboriginXHRTest(
  false,
  'Complex anonymous XHR preflight with \'*\' ACAO, no AC for custom header',
  xorigin_preflight_script + '?cors=*',
  'anonymous').execute();

new SuboriginXHRTest(
  true,
  'Complex anonymous XHR preflight with \'*\' ACAO, has AC for custom header',
  xorigin_preflight_script + '?cors=*&custom=x-custom-header',
  'anonymous').execute();

new SuboriginXHRTest(
  false,
  'Complex XHR with credentials preflight, no AC for custom header',
  xorigin_preflight_script + '?cors=http-so://foobar.127.0.0.1:8000&' +
  'credentials=true',
  'use-credentials').execute();

new SuboriginXHRTest(
  true,
  'Complex XHR with credentials preflight, has AC for custom header',
  xorigin_preflight_script + '?cors=http-so://foobar.127.0.0.1:8000&' +
  'credentials=true&custom=x-custom-header',
  'use-credentials').execute();

new SuboriginXHRTest(
  false,
  'Complex XHR with credentials preflight with \'*\' ACAO, ' +
  'no AC for custom header',
  xorigin_preflight_script + '?cors=*&credentials=true',
  'use-credentials').execute();

new SuboriginXHRTest(
  false,
  'Complex XHR with credentials preflight with \'*\' ACAO, ' +
  'has AC for custom header',
  xorigin_preflight_script + '?cors=*&credentials=true&custom=x-custom-header',
  'use-credentials').execute();
</script>
</body>
</html>
