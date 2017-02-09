// Notes about generated waveforms:
//
// QUESTION: Why does the wave shape not look like the exact shape (sharp edges)?
// ANSWER: Because a shape with sharp edges has infinitely high frequency content.
// Since a digital audio signal must be band-limited based on the nyquist frequency (half the sample-rate)
// in order to avoid aliasing, this creates more rounded edges and "ringing" in the
// appearance of the waveform.  See Nyquist-Shannon sampling theorem:
// http://en.wikipedia.org/wiki/Nyquist%E2%80%93Shannon_sampling_theorem
//
// QUESTION: Why does the very end of the generated signal appear to get slightly weaker?
// ANSWER: This is an artifact of the algorithm to avoid aliasing.
//
// QUESTION: Since the tests compare the actual result with an expected reference file, how are the
// reference files created?
// ANSWER: Create an html with the following contents in the webaudio directory.  Then run a layout
// test on this file.  A new file names "<file>-actual.wav" is created that contains the new result
// that can be used as the new expected reference file.  Replace the "sine" below with the
// oscillator type that you want to use.
//
// <!doctype html>
// <html>
// <head>
// <script src="../resources/testharness.js"></script>
// <script src="resources/oscillator-testing.js"></script>
// <script src="resources/audit-util.js"></script>
// </head>
// <body>
// <script>
//   OscillatorTestingUtils.createNewReference("sine");
// </script>
// </body>
// </html>

OscillatorTestingUtils = (function () {

var sampleRate = 44100.0;
var nyquist = 0.5 * sampleRate;
var lengthInSeconds = 4;
var lowFrequency = 10;
var highFrequency = nyquist + 2000; // go slightly higher than nyquist to make sure we generate silence there
var context = 0;

// Scaling factor for converting the 16-bit WAV data to float (and vice-versa).
var waveScaleFactor = 32768;

// Thresholds for verifying the test passes.  The thresholds are experimentally determined. The
// default values here will cause the test to fail, which is useful for determining new thresholds,
// if needed.

// SNR must be greater than this to pass the test.
// Q: Why is the SNR threshold not infinity?
// A: The reference result is a 16-bit WAV file, so it won't compare exactly with the
//    floating point result.
var thresholdSNR = 10000;

// Max diff must be less than this to pass the test.
var thresholdDiff = 0;

// Count the number of differences between the expected and actual result. The tests passes
// if the count is less than this threshold.
var thresholdDiffCount = 0;

// Mostly for debugging

// An AudioBuffer for the reference (expected) result.
var reference = 0;

// Signal power of the reference
var signalPower = 0;

// Noise power of the difference between the reference and actual result.
var noisePower = 0;

function generateExponentialOscillatorSweep(context, oscillatorType) {
    var osc = context.createOscillator();
    if (oscillatorType == "custom") {
        // Create a simple waveform with three Fourier coefficients.
        // Note the first values are expected to be zero (DC for coeffA and Nyquist for coeffB).
        var coeffA = new Float32Array([0, 1, 0.5]);
        var coeffB = new Float32Array([0, 0, 0]);
        var wave = context.createPeriodicWave(coeffA, coeffB);
        osc.setPeriodicWave(wave);
    } else {
        osc.type = oscillatorType;
    }

    // Scale by 1/2 to better visualize the waveform and to avoid clipping past full scale.
    var gainNode = context.createGain();
    gainNode.gain.value = 0.5;
    osc.connect(gainNode);
    gainNode.connect(context.destination);

    osc.start(0);

    osc.frequency.setValueAtTime(10, 0);
    osc.frequency.exponentialRampToValueAtTime(highFrequency, lengthInSeconds);
}

function calculateSNR(sPower, nPower)
{
    if (nPower == 0 && sPower > 0) {
        return 1000;
    }
    return 10 * Math.log10(sPower / nPower);
}

function loadReferenceAndRunTest(context, oscType, task, should) {
     var bufferLoader = new BufferLoader(
        context,
        [ "../Oscillator/oscillator-" + oscType + "-expected.wav" ],
        function (bufferList) {
            reference = bufferList[0].getChannelData(0);
            generateExponentialOscillatorSweep(context, oscType);
            context.oncomplete = () => {
              checkResult(event, should);
              task.done();
            };
            context.startRendering();
        });

    bufferLoader.load();
}

function checkResult (event, should) {
    let renderedData = event.renderedBuffer.getChannelData(0);
    // Compute signal to noise ratio between the result and the reference. Also keep track
    // of the max difference (and position).

    var maxError = -1;
    var errorPosition = -1;
    var diffCount = 0;

    for (var k = 0; k < renderedData.length; ++k) {
        var diff = renderedData[k] - reference[k];
        noisePower += diff * diff;
        signalPower += reference[k] * reference[k];
        if (Math.abs(diff) > maxError) {
            maxError = Math.abs(diff);
            errorPosition = k;
        }
        // The reference file is a 16-bit WAV file, so we will almost never get an exact match
        // between it and the actual floating-point result.
        if (diff > 1/waveScaleFactor) {
            diffCount++;
        }
    }

    var snr = calculateSNR(signalPower, noisePower);
    should(snr, "SNR")
        .beGreaterThanOrEqualTo(thresholdSNR);
    should(maxError * waveScaleFactor, "Maximum difference in ulp (16-bits)")
        .beLessThanOrEqualTo(thresholdDiff * waveScaleFactor);

    should(diffCount,
           "Number of differences between actual and expected result out of "
           + renderedData.length + " frames")
        .beLessThanOrEqualTo(thresholdDiffCount);
}

function setThresholds(thresholds) {
    thresholdSNR = thresholds.snr;
    thresholdDiff = thresholds.maxDiff / waveScaleFactor;
    thresholdDiffCount = thresholds.diffCount;
}

function runTest(context, oscType, description, task, should) {
  loadReferenceAndRunTest(context, oscType, task, should);
}

function createNewReference(oscType) {
     if (!window.testRunner)
         return;

     context = new OfflineAudioContext(1, sampleRate * lengthInSeconds, sampleRate);
     generateExponentialOscillatorSweep(context, oscType);

     context.oncomplete = finishAudioTest;
     context.startRendering();

     testRunner.waitUntilDone();
}

return {
    sampleRate: sampleRate,
    lengthInSeconds: lengthInSeconds,
    thresholdSNR: thresholdSNR,
    thresholdDiff: thresholdDiff,
    thresholdDiffCount: thresholdDiffCount,
    waveScaleFactor: waveScaleFactor,
    setThresholds: setThresholds,
    runTest: runTest,
    createNewReference: createNewReference,
};

}());
