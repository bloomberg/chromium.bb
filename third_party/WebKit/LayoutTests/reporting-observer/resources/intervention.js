async_test(function(test) {
  var observer = new ReportingObserver(function(reports, observer) {
    test.step(function() {
      assert_equals(reports.length, 1);

      // Ensure that the contents of the report are valid.
      assert_equals(report.type, "intervention");
      assert_true(report.url.endsWith(
          "reporting-observer/intervention.html"));
      assert_true(report.body.sourceFile.endsWith(
          "reporting-observer/resources/intervention.js"));
      assert_equals(typeof reports[0].body.lineNumber, "number");
      assert_equals(typeof reports[0].body.message, "string");
    });

    test.done();
  });
  observer.observe();

  // Cause an intervention.

  var target = document.getElementById('target');
  var rect = target.getBoundingClientRect();
  var targetX = rect.left + rect.width / 2;
  var targetY = rect.top + rect.height / 2;
  document.body.addEventListener('touchstart', function(e) {
    e.preventDefault();
    test.done();
  });

  var touches = [new Touch({identifier: 1, clientX: targetX, clientY: targetY, target: target})];
  var touchEventInit = {
    cancelable: false,
    touches: touches,
    targetTouches: touches,
    changedTouches: touches,
    view: window
  };
  var event = new TouchEvent('touchstart', touchEventInit);


  var deadline = performance.now() + 100;
  while (performance.now() < deadline) {};


  document.body.dispatchEvent(event);
}, "Intervention report");
