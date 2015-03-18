function runLateStartTest(audit, context, node) {

  // Set up a dummy signal path to keep the audio context running and spend
  // processing time before calling start(0).
  var osc = context.createOscillator();
  var silent = context.createGain();

  osc.connect(silent);
  silent.connect(context.destination);
  silent.gain.setValueAtTime(0.0, 0);
  osc.start();

  node.connect(context.destination);

  // Task: define |onstatechange| and start rendering.
  audit.defineTask('test-late-start', function (done) {

    // Trigger playback at 0 second. The assumptions are:
    //
    // 1) The specified timing of start() call is already passed in terms of
    // the context time. So the argument |0| will be clamped to the current
    // context time.
    // 2) The |onstatechange| event will be fired later than the 0 second
    // context time.
    //
    // See issue: crbug.com/462167
    context.onstatechange = function () {
      if (context.state === 'running') {
        node.start(0);
      }
    };

    // Start rendering and verify result: this verifies if 1) the rendered
    // buffer contains at least one non-zero value and 2) the non-zero value is
    // found later than the first output sample.
    context.startRendering().then(function (buffer) {

      var nonZeroValueIndex = -1;
      var channelData = buffer.getChannelData(0);
      for (var i = 0; i < channelData.length; i++) {
        if (channelData[i] !== 0) {
          nonZeroValueIndex = i;
          break;
        }
      }

      if (nonZeroValueIndex === -1) {
        testFailed('The rendered buffer was all zeros.');
      } else if (nonZeroValueIndex === 0) {
        testFailed('The first sample was non-zero value. It should be zero.');
      } else {
        testPassed('The rendered buffer contains non-zero values after the first sample.');
      }

      done();
    });
  });

  audit.defineTask('finish-test', function (done) {
    done();
    finishJSTest();
  });

  audit.runTasks(
    'test-late-start',
    'finish-test'
  );

}