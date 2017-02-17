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

  // Task: schedule a suspend and start rendering.
  audit.defineTask('test-late-start', function (done) {
    // The node's start time will be clamped to the render quantum boundary
    // >0.1 sec. Thus the rendered buffer will have non-zero frames.
    // See issue: crbug.com/462167
    context.suspend(0.1).then(() => {
      node.start(0);
      context.resume();
    });

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

      var success =
          Should('The index of first non-zero value',nonZeroValueIndex)
              .notBeEqualTo(-1);
      success = Should('The first sample value', channelData[0])
          .beEqualTo(0) && success;
      Should('The rendered buffer', success)
          .summarize('contains non-zero values after the first sample',
                     'was all zeros or has non-zero first sample.');
      done();
    });
  });

  audit.runTasks();
}
