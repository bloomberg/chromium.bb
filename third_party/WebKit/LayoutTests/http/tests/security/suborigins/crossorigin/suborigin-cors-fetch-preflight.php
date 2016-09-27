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
// Fetch tests
var SuboriginFetchTest = function(pass, name, src, crossorigin_value) {
  SuboriginTest.call(this, pass, 'Fetch: ' + name, src, crossorigin_value);
}

SuboriginFetchTest.prototype.execute = function() {
  var settings = this;
  async_test(test => {
      var headers = new Headers();
      // Set a custom header to force a preflight. Even though the
      // scheme/host/port of the source and destination origins are the same,
      // the Suborigin should cause the request to be treated as cross-origin.
      headers.append('x-custom-header', 'foobar');
      var options = {
        method: 'GET',
        headers: headers,
        credentials: (settings.crossorigin_value === 'use-credentials') ?
                     'include' :
                     'exclude',
        mode: 'cors'
      }

      var response_func;
      var error_func;
      if (settings.pass) {
        response_func = test.step_func_done();
        error_func = test.unreached_func('Good Fetch fired error handler.');
      } else {
        response_func = test.unreached_func('Bad Fetch successful.');
        error_func = test.step_func_done();
      }
      fetch(settings.src, options)
        .then(response_func)
        .catch(error_func);
    }, settings.name);
};

var xorigin_preflight_script =
  'http://127.0.0.1:8000/security/resources/cors-script.php';

// Fetch preflight tests
new SuboriginFetchTest(
  false,
  'Complex anonymous Fetch preflight, no AC for custom header',
  xorigin_preflight_script + '?cors=http-so://foobar.127.0.0.1:8000',
  'anonymous').execute();

new SuboriginFetchTest(
  true,
  'Complex anonymous Fetch preflight, has AC for custom header',
  xorigin_preflight_script +
  '?cors=http-so://foobar.127.0.0.1:8000&custom=x-custom-header',
  'anonymous').execute();

new SuboriginFetchTest(
  false,
  'Complex anonymous Fetch preflight with \'*\' ACAO, no AC for custom header',
  xorigin_preflight_script + '?cors=*',
  'anonymous').execute();

new SuboriginFetchTest(
  true,
  'Complex anonymous Fetch preflight with \'*\' ACAO, has AC for custom header',
  xorigin_preflight_script + '?cors=*&custom=x-custom-header',
  'anonymous').execute();

new SuboriginFetchTest(
  false,
  'Complex Fetch with credentials preflight, no AC for custom header',
  xorigin_preflight_script +
  '?cors=http-so://foobar.127.0.0.1:8000&credentials=true',
  'use-credentials').execute();

new SuboriginFetchTest(
  true,
  'Complex Fetch with credentials preflight, has AC for custom header',
  xorigin_preflight_script +
  '?cors=http-so://foobar.127.0.0.1:8000&credentials=true&' +
  'custom=x-custom-header',
  'use-credentials').execute();

new SuboriginFetchTest(
  false,
  'Complex Fetch with credentials preflight with \'*\' ACAO, ' +
  'no AC for custom header',
  xorigin_preflight_script + '?cors=*&credentials=true',
  'use-credentials').execute();

new SuboriginFetchTest(
  false,
  'Complex Fetch with credentials preflight with \'*\' ACAO, ' +
  'has AC for custom header',
  xorigin_preflight_script + '?cors=*&credentials=true&custom=x-custom-header',
  'use-credentials').execute();
</script>
</body>
</html>
