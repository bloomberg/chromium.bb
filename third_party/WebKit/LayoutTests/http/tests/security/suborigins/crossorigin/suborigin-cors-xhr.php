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
      xhr.send();
    }, settings.name);
};

// XHR tests
new SuboriginXHRTest(
  false,
  '<crossorigin=\'anonymous\'>, ACAO: ' + server,
  xoriginAnonScript(),
  'anonymous').execute();

new SuboriginXHRTest(
  true,
  '<crossorigin=\'anonymous\'>, ACAO: *',
  xoriginAnonScript('*'),
  'anonymous').execute();

new SuboriginXHRTest(
  false,
  '<crossorigin=\'use-credentials\'>, ACAO: ' + server,
  xoriginCredsScript(),
  'use-credentials').execute();

new SuboriginXHRTest(
  false,
  '<crossorigin=\'anonymous\'>, CORS-ineligible resource',
  xoriginIneligibleScript(),
  'anonymous').execute();
</script>
</body>
</html>
