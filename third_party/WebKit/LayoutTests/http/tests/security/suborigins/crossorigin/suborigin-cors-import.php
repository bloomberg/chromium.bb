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
var import_tests = [];
import_tests.next = function() {
  if (import_tests.length > 0) {
    import_tests.shift().execute();
  }
}

var SuboriginHTMLImportTest = function(pass, name, href, crossorigin_value) {
    var import_fail_verify = function(div, e) {
        assert_equals(e.import, null);
    };
    SuboriginLinkTest.call(
      this, import_tests, 'import', function() { }, import_fail_verify,
      pass, 'Import: ' + name, href, crossorigin_value);
};
SuboriginHTMLImportTest.prototype = Object.create(SuboriginLinkTest.prototype);

add_result_callback(function(res) {
    if (res.name.startsWith('Style: ') || res.name.startsWith('Import: ')) {
      import_tests.next();
    }
});

// HTML Imports tests
new SuboriginHTMLImportTest(
  false,
  '<crossorigin=\'anonymous\'>, ACAO: ' + server,
  xoriginAnonHello(),
  'anonymous');

new SuboriginHTMLImportTest(
  true,
  '<crossorigin=\'anonymous\'>, ACAO: *',
  xoriginAnonHello('*'),
  'anonymous');

new SuboriginHTMLImportTest(
  false,
  '<crossorigin=\'use-credentials\'>, ACAO: ' + server,
  xoriginCredsHello(),
  'use-credentials');

new SuboriginHTMLImportTest(
  false,
  '<crossorigin=\'anonymous\'>, CORS-ineligible resource',
  xoriginIneligibleHello(),
  'anonymous');

import_tests.next();
</script>
</body>
</html>
