function createTestBuffer(context, sampleFrameLength) {
    var audioBuffer = context.createBuffer(1, sampleFrameLength, context.sampleRate);
    var channelData = audioBuffer.getChannelData(0);

    // Create a simple linear ramp starting at zero, with each value in the buffer equal to its index position.
    for (var i = 0; i < sampleFrameLength; ++i)
        channelData[i] = i;

    return audioBuffer;
}

function checkSingleTest(renderedBuffer, i) {
    var renderedData = renderedBuffer.getChannelData(0);
    var offsetFrame = i * testSpacingFrames;

    var test = tests[i];
    var expected = test.expected;
    var success = true;
    var description;

    if (test.description) {
        description = test.description;
    } else {
        // No description given, so create a basic one from the given test parameters.
        description = "loop from " + test.loopStartFrame + " -> " + test.loopEndFrame;
        if (test.offsetFrame)
            description += " with offset " + test.offsetFrame;
        if (test.playbackRate && test.playbackRate != 1)
            description += " with playbackRate of " + test.playbackRate;
    }

    var framesToTest;

    if (test.renderFrames)
        framesToTest = test.renderFrames;
    else if (test.durationFrames)
        framesToTest = test.durationFrames;

    // Verify that the output matches
    for (var j = 0; j < framesToTest; ++j) {
        if (expected[j] != renderedData[offsetFrame + j]) {
            // Copy from Float32Array to regular JavaScript array for error message.
            var renderedArray = new Array();
            for (var j = 0; j < test.renderFrames; ++j)
                renderedArray[j] = renderedData[offsetFrame + j];

            var s = description + ": expected: " + expected + " actual: " + renderedArray;
            testFailed(s);
            success = false;
            break;
        }
    }

    // Verify that we get all zeroes after the buffer (or duration) has passed.
    for (var j = framesToTest; j < testSpacingFrames; ++j) {
        if (renderedData[offsetFrame + j]) {
            // Copy from Float32Array to regular JavaScript array for error message.
            var renderedArray = new Array();
            for (var j = framesToTest; j < testSpacingFrames; ++j)
                renderedArray[j - framesToTest] = renderedData[offsetFrame + j];

            var s = description + ": expected: all zeroes actual: " + renderedArray;
            testFailed(s);
            success = false;
            break;
        }
    }

    if (success)
        testPassed(description);

    return success;
}

function checkAllTests(event) {
    var renderedBuffer = event.renderedBuffer;
    for (var i = 0; i < tests.length; ++i)
        checkSingleTest(renderedBuffer, i);

    finishJSTest();
}


// Create the actual result by modulating playbackRate or detune AudioParam of
// ABSN. |modTarget| is a string of AudioParam name, |modOffset| is the offset
// (anchor) point of modulation, and |modRange| is the range of modulation.
//
//   createSawtoothWithModulation(context, 'detune', 440, 1200);
//
// The above will perform a modulation on detune within the range of
// [1200, -1200] around the sawtooth waveform on 440Hz.
function createSawtoothWithModulation(context, modTarget, modOffset, modRange) {
  var lfo = context.createOscillator();
  var amp = context.createGain();

  // Create a sawtooth generator with the signal range of [0, 1].
  var phasor = context.createBufferSource();
  var phasorBuffer = context.createBuffer(1, sampleRate, sampleRate);
  var phasorArray = phasorBuffer.getChannelData(0);
  var phase = 0, phaseStep = 1 / sampleRate;
  for (var i = 0; i < phasorArray.length; i++) {
    phasorArray[i] = phase % 1.0;
    phase += phaseStep;
  }
  phasor.buffer = phasorBuffer;
  phasor.loop = true;

  // 1Hz for audible (human-perceivable) parameter modulation by LFO.
  lfo.frequency.value = 1.0;

  amp.gain.value = modRange;
  phasor.playbackRate.value = modOffset;

  // The oscillator output should be amplified accordingly to drive the
  // modulation within the desired range.
  lfo.connect(amp);
  amp.connect(phasor[modTarget]);

  phasor.connect(context.destination);

  lfo.start();
  phasor.start();
}