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

    for (var j = 0; j < test.renderFrames; ++j) {
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

    if (success)
        testPassed(description);

    return success;
}

function checkAllTests(event) {
    var renderedBuffer = event.renderedBuffer;
    for (var i = 0; i < tests.length; ++i)
        checkSingleTest(renderedBuffer, i);

    if (window.testRunner)
        testRunner.notifyDone()
}
