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
// FontFace tests
var SuboriginFontFaceTest = function(pass, name, src) {
  SuboriginTest.call(this, pass, 'FontFace: ' + name, src, false);
};

SuboriginFontFaceTest.prototype.execute = function() {
  var settings = this;
  async_test(test => {
      var url = 'url(\'' + settings.src + '\')';
      var fontFace = new FontFace('ahem', url);

      if (settings.pass) {
        fontFace.load()
          .then(function(loadedFace) {
              test.step(function() {
                  assert_equals(loadedFace.family, 'ahem');
                  test.done();
                })},
            function() {
              test.step(function() {
                  assert_unreached('Good FontFace failed to load.');
                  test.done();
                });
            });
      } else {
        fontFace.load()
          .then(function() {
              test.step(function() {
                  assert_unreached('Bad FontFace loaded.');
                  test.done();
                })},
            function() {
              test.step(function() {
                  test.done();
                });
            });
      }
    }, settings.name);
};

// CSS FontFace tests
new SuboriginFontFaceTest(
  false,
  'ACAO: ' + server,
  xoriginFont()).execute();
new SuboriginFontFaceTest(
  true,
  'ACAO: *',
  xoriginFont('*')).execute();
new SuboriginFontFaceTest(
  false,
  'CORS-ineligible resource',
  xoriginIneligibleFont()).execute();
</script>
</body>
</html>
