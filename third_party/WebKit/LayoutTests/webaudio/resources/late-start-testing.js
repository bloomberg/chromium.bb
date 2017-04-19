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
  audit.define('test-late-start', (task, should) => {
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

      should(nonZeroValueIndex, 'The index of first non-zero value')
              .notBeEqualTo(-1);
      should(channelData[0], 'The first sample value')
          .beEqualTo(0);
      task.done();
    });
  });

  audit.run();
}
