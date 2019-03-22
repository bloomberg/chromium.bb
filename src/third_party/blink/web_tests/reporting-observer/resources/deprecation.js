async_test(function(test) {
  // UseCounter feature IDs, from web_feature.mojom.
  var kReportingObserver = 2529;
  var kDeprecationReport = 2530;

  var observer = new ReportingObserver(function(reports, observer) {
    test.step(function() {
      assert_equals(reports.length, 2);
      assert_true(internals.isUseCounted(document, kDeprecationReport));

      // Ensure that the contents of the reports are valid.
      for(let report of reports) {
        assert_equals(report.type, "deprecation");
        assert_true(report.url.endsWith(
            "reporting-observer/deprecation.html"));
        assert_equals(typeof report.body.id, "string");
        assert_equals(typeof report.body.anticipatedRemoval, "object");
        assert_equals(typeof report.body.message, "string");
        assert_true(report.body.sourceFile.endsWith(
            "reporting-observer/resources/deprecation.js"));
        assert_equals(typeof report.body.lineNumber, "number");
        assert_equals(typeof report.body.columnNumber, "number");
      }
      assert_not_equals(reports[0].body.id, reports[1].body.id);
    });

    test.done();
  });
  assert_false(internals.isUseCounted(document, kReportingObserver));
  observer.observe();
  assert_true(internals.isUseCounted(document, kReportingObserver));

  assert_false(internals.isUseCounted(document, kDeprecationReport));

  // This ensures that ReportingObserver is traced properly. This will cause the
  // test to fail otherwise.
  window.gc();

  // Use two deprecated features to generate two deprecation reports.
  window.webkitStorageInfo;
  window.webkitRequestAnimationFrame(() => {});
}, "Deprecation reports");
