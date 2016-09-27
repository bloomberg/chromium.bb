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
var style_tests = [];
style_tests.next = function() {
  if (style_tests.length > 0) {
    style_tests.shift().execute();
  }
}

var SuboriginStyleTest = function(pass, name, href, crossorigin_value) {
  var style_pass_verify = function (div, e) {
      var background =
        window.getComputedStyle(div, null).getPropertyValue('background-color');
      assert_equals(background, 'rgb(255, 255, 0)');
  };

  var style_fail_verify = function(div, e) {
      var background =
        window.getComputedStyle(div, null).getPropertyValue('background-color');
      assert_equals(background, 'rgba(0, 0, 0, 0)');
  };

  SuboriginLinkTest.call(
    this, style_tests, 'stylesheet', style_pass_verify,
    style_fail_verify, pass, 'Style: ' + name, href, crossorigin_value);
};
SuboriginStyleTest.prototype = Object.create(SuboriginLinkTest.prototype);

add_result_callback(function(res) {
    if (res.name.startsWith('Style: ') || res.name.startsWith('Import: ')) {
      style_tests.next();
    }
  });

// Style tests
new SuboriginStyleTest(
    false,
    '<crossorigin="anonymous">, ACAO: ' + server,
    xoriginAnonStyle(),
    'anonymous');

new SuboriginStyleTest(
    true,
    '<crossorigin="anonymous">, ACAO: *',
    xoriginAnonStyle('*'),
    'anonymous');

new SuboriginStyleTest(
    false,
    '<crossorigin="use-credentials">, ACAO: ' + server,
    xoriginCredsStyle(),
    'use-credentials');

new SuboriginStyleTest(
    false,
    '<crossorigin="anonymous">, CORS-ineligible resource',
    xoriginIneligibleStyle(),
    'anonymous');

style_tests.next();
</script>
</body>
</html>
