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
      var options = {};
      if (settings.crossorigin_value === 'same-origin') {
        options.mode = 'same-origin';
      } else if (settings.crossorigin_value === 'anonymous') {
        options.mode = 'cors';
      } else if (settings.crossorigin_value === 'use-credentials') {
        options.mode = 'cors';
        options.credentials = 'include';
      }

      var pass = this.pass;
      fetch(this.src, options)
        .then(function(response) {
            if (pass) {
              test.step(function() { test.done(); });
            } else {
              test.step(function() {
                  assert_unreached('Bad Fetch succeeded.');
                  test.done();
                });
            }
          })
        .catch(function(error) {
            if (pass) {
              test.step(function() {
                  assert_unreached('Good Fetch failed.');
                  test.done();
                });
            } else {
              test.step(function() { test.done(); });
            }
          });
    }, settings.name);
};

// Fetch tests
new SuboriginFetchTest(
  false,
  'anonymous, ACAO: ' + server,
  xoriginAnonScript(),
  'anonymous').execute();

new SuboriginFetchTest(
  true,
  'anonymous, ACAO: *',
  xoriginAnonScript('*'),
  'anonymous').execute();

new SuboriginFetchTest(
  false,
  'use-credentials, ACAO: ' + server,
  xoriginCredsScript(),
  'use-credentials').execute();

new SuboriginFetchTest(
  false,
  'anonymous, CORS-ineligible resource',
  xoriginIneligibleScript(),
  'anonymous').execute();

new SuboriginFetchTest(
  false,
  'same-origin, ACAO: * ',
  xoriginAnonScript('*'),
  'same-origin').execute();
</script>
</body>
</html>
