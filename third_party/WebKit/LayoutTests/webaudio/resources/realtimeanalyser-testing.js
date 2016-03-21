function createGraph(options) {
    var context = new OfflineAudioContext(1, renderFrames, sampleRate);

    // Use a default sawtooth wave for the test signal.  We want something is a
    // bit more harmonic content than a sine wave, but otherwise it doesn't
    // really matter.
    var src = context.createOscillator();
    src.type = "sawtooth";

    var analyser = context.createAnalyser();
    analyser.fftSize = Math.pow(2, options.order);
    analyser.smoothingTimeConstant = options.smoothing || 0;
    analyser.minDecibels = options.minDecibels || analyser.minDecibels;

    // Connect the nodes together and start the source.
    src.connect(analyser);
    analyser.connect(context.destination);

    src.start();

    return {
        context: context,
        analyser: analyser,
    };
}

// Apply the windowing function, in place.
function applyWindow(timeData) {
    var length = timeData.length;
    var alpha = 0.16;
    var a0 = (1 - alpha) / 2;
    var a1 = 0.5;
    var a2 = alpha / 2;
    var omega = 2 * Math.PI / length;

    for (var k = 0; k < length; ++k) {
        var w = a0 - a1 * Math.cos(omega * k) + a2 * Math.cos(2 * omega *
            k);
        timeData[k] *= w;
    }
}

// Compute the FFT magnitude of |timeData|.
function computeFFTMagnitude(timeData, order) {
    // Compute the expected frequency response.  First, apply the window.
    // Compute the forward FFT.
    applyWindow(timeData);

    var fft = new FFT(order);
    var fftSize = Math.pow(2, order);
    var fftr = new Float32Array(fftSize);
    var ffti = new Float32Array(fftSize);
    fft.rfft(timeData, fftr, ffti);

    // Compute the magnitude of the expected result.
    var expected = new Float32Array(fftSize / 2);
    for (var k = 0; k < expected.length; ++k)
        expected[k] = Math.hypot(fftr[k], ffti[k]) / fftSize;

    return expected;
}

// Convert dB value to linear value.
function dbToLinear(x) {
    return Math.pow(10, x / 20);
}

// Convert linear value to dB.
function linearToDb(x) {
    return 20 * Math.log10(x);
}

// Clip the FFT magnitude so that values below |limit| are set to |limit|.  The
// FFT must be in dB.  The input array is clipped in place.
function clipMagnitude(limit, x) {
    for (var k = 0; k < x.length; ++k)
        x[k] = Math.max(limit, x[k])
}

// Compare the float frequency data in dB, |freqData|, against the expected value,
// |expectedFreq|. |options| is a dictionary with the property |floatRelError| for setting the
// comparison threshold and |precision| for setting the printed precision.  Setting |precision| to
// |undefined| means printing all digits.  If |options.precision} doesn't exist, use a default
// precision.
function compareFloatFreq(message, freqData, expectedFreq, options) {
    // Any dB values below -100 is pretty much in the noise due to round-off in
    // the (single-precisiion) FFT, so just clip those values to -100.
    var lowerLimit = -100;
    clipMagnitude(lowerLimit, expectedFreq);

    var actual = freqData;
    clipMagnitude(lowerLimit, actual);

    // Default precision for printing the FFT data.  Distinguish between options.precision existing
    // or not.  If it does, use whatever value is there, including undefined.
    var defaultPrecision = options.hasOwnProperty("precision") ? options.precision : 3;

    var success = Should(message, actual, {
            verbose: true,
            precision: defaultPrecision
        })
        .beCloseToArray(expectedFreq, {
            relativeThreshold: options.floatRelError || 0,
        });

    return {
        success: success,
        expected: expectedFreq
    };
}

// Apply FFT smoothing, accumulating the result in |oldFreqData| with the new
// data in |newFreqData|.  The smoothing time constant is |smoothingTime|
function smoothFFT(oldFreqData, newFreqData, smoothingTime) {
    for (var k = 0; k < oldFreqData.length; ++k) {
        var value = smoothingTime * oldFreqData[k] + (1 - smoothingTime) *
            newFreqData[k];
        oldFreqData[k] = value;
    }
}

// Convert the float frequency data, |floatFreqData|, to byte values using the
// dB limits |minDecibels| and |maxDecibels|.  The new byte array is returned.
function convertFloatToByte(floatFreqData, minDecibels, maxDecibels) {
    var scale = 255 / (maxDecibels - minDecibels);

    return floatFreqData.map(function (x) {
        var value = Math.floor(scale * (x - minDecibels));
        return Math.min(255, Math.max(0, value));
    });
}
