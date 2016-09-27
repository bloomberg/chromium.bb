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
var SuboriginScriptTest = function(pass, name, src, crossorigin_value) {
  SuboriginTest.call(this, pass, 'Script: ' + name, src, crossorigin_value);
}

SuboriginScriptTest.prototype.execute = function() {
  var settings = this;
  async_test(test => {
      var e = document.createElement('script');
      e.src = settings.src;
      if (settings.crossorigin_value) {
        e.setAttribute('crossorigin', settings.crossorigin_value);
      }
      if (settings.pass) {
        e.addEventListener('load', test.step_func_done());
        e.addEventListener(
          'error', test.unreached_func('Good load fired error handler.'));
      } else {
        e.addEventListener('load', test.unreached_func('Bad load successful.'));
        e.addEventListener('error', test.step_func_done());
      }
      document.body.appendChild(e);
    }, settings.name);
}

// This is unused but required by cors-script.php.
window.result = false;

// Script tests
new SuboriginScriptTest(
  false,
  '<crossorigin=\'anonymous\'>, ACAO: ' + server,
  xoriginAnonScript(),
  'anonymous').execute();

new SuboriginScriptTest(
  true,
  '<crossorigin=\'anonymous\'>, ACAO: *',
  xoriginAnonScript('*'),
  'anonymous').execute();

new SuboriginScriptTest(
  false,
  '<crossorigin=\'use-credentials\'>, ACAO: ' + server,
  xoriginCredsScript(),
  'use-credentials').execute();

new SuboriginScriptTest(
  false,
  '<crossorigin=\'anonymous\'>, CORS-ineligible resource',
  xoriginIneligibleScript(),
  'anonymous').execute();
</script>
</body>
</html>
