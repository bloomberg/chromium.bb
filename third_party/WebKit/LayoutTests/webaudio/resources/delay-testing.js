var sampleRate = 44100.0;

var renderLengthSeconds = 4;
var delayTimeSeconds = 0.5;
var toneLengthSeconds = 2;

function createToneBuffer(context, frequency, numberOfCycles, sampleRate) {
    var duration = numberOfCycles / frequency;
    var sampleFrameLength = duration * sampleRate;
    
    var audioBuffer = context.createBuffer(1, sampleFrameLength, sampleRate);

    var n = audioBuffer.length;
    var data = audioBuffer.getChannelData(0);

    for (var i = 0; i < n; ++i)
        data[i] = Math.sin(frequency * 2.0*Math.PI * i / sampleRate);

    return audioBuffer;
}

function checkDelayedResult(renderedBuffer, toneBuffer, should) {
    var sourceData = toneBuffer.getChannelData(0);
    var renderedData = renderedBuffer.getChannelData(0);

    var delayTimeFrames = delayTimeSeconds * sampleRate;
    var toneLengthFrames = toneLengthSeconds * sampleRate;

    var success = true;

    var n = renderedBuffer.length;

    for (var i = 0; i < n; ++i) {
        if (i < delayTimeFrames) {
            // Check that initial portion is 0 (since signal is delayed).
            if (renderedData[i] != 0) {
                should(renderedData[i],
                       "Initial portion expected to be 0 at frame " + i)
                    .beEqualTo(0);
                success = false;
                break;
            }
        } else if (i >= delayTimeFrames && i < delayTimeFrames +
            toneLengthFrames) {
            // Make sure that the tone data is delayed by exactly the expected number of frames.
            var j = i - delayTimeFrames;
            if (renderedData[i] != sourceData[j]) {
                should(renderedData[i],
                     "Actual data at frame " + i)
                  .beEqualTo(sourceData[j]);
                success = false;
                break;
            }
        } else {
            // Make sure we have silence after the delayed tone.
            if (renderedData[i] != 0) {
                should(renderedData[j],
                     "Final portion at frame " + i)
                  .beEqualTo(0);
                success = false;
                break;
            }
        }
    }

    should(success, "Delaying test signal by " + delayTimeSeconds + " sec was done")
        .message("correctly", "incorrectly")
}
