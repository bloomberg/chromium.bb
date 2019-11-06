<?php
header("Content-Security-Policy: img-src 'none';");
?>
<!doctype html>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>

<img id="target">

<script>
async_test(function(test) {
  var observer = new ReportingObserver(function(reports, observer) {
    test.step(function() {
      assert_equals(reports.length, 1);

      // Ensure that the contents of the report are valid.
      assert_equals(reports[0].type, "csp-violation");
      assert_true(reports[0].url.endsWith("reporting-observer/csp.php"));
      assert_true(reports[0].body.documentURL.endsWith(
          "reporting-observer/csp.php"));
      assert_equals(reports[0].body.referrer, null);
      assert_true(reports[0].body.blockedURL.endsWith(
          "reporting-observer/fail.png"));
      assert_equals(reports[0].body.effectiveDirective, "img-src");
      assert_equals(reports[0].body.originalPolicy,
                    "img-src 'none';");
      assert_equals(reports[0].body.sourceFile, null);
      assert_equals(reports[0].body.sample, null);
      assert_equals(reports[0].body.disposition, "enforce");
      assert_equals(reports[0].body.statusCode, 200);
      assert_equals(reports[0].body.lineNumber, null);
      assert_equals(reports[0].body.columnNumber, null);
    });

    test.done();
  });
  observer.observe();

  // Attempt to load an image, which is disallowed by the content security
  // policy. This will generate a csp-violation report.
  document.getElementById("target").src = "fail.png";
}, "CSP Report");
</script>
