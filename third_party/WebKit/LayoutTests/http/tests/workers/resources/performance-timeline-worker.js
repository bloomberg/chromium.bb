importScripts('../../resources/testharness.js');

promise_test(function(test) {
    var durationMsec = 100;

    return new Promise(function(resolve) {
        performance.mark('startMark');
        setTimeout(resolve, durationMsec);
      }).then(function() {
          performance.mark('endMark');
          performance.measure('measure', 'startMark', 'endMark');

          var startMark = performance.getEntriesByName('startMark')[0];
          var endMark = performance.getEntriesByName('endMark')[0];
          var measure = performance.getEntriesByType('measure')[0];

          assert_equals(measure.startTime, startMark.startTime);
          assert_approx_equals(endMark.startTime - startMark.startTime,
                               measure.duration, 0.001);
          assert_greater_than(measure.duration, durationMsec);

          assert_equals(performance.getEntriesByType('mark').length, 2);
          assert_equals(performance.getEntriesByType('measure').length, 1);
          performance.clearMarks('startMark');
          performance.clearMeasures('measure');
          assert_equals(performance.getEntriesByType('mark').length, 1);
          assert_equals(performance.getEntriesByType('measure').length, 0);
      });
  }, 'User Timing');

done();
